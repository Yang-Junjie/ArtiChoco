#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <memory>

namespace enki {
class TaskScheduler;
class TaskSet;
class LambdaPinnedTask;
} // namespace enki

namespace arti::core {

namespace detail {
class TaskPool;
} // namespace detail

// 进程级配置，只在 init() 时读一次。刻意不做惰性初始化：线程数这类东西是**进程启动时**的
// 决定，惰性建意味着谁先碰它谁定配置。
struct TaskSystemConfig {
    // worker 线程数。0 = 用 enkiTS 自己的默认（hardware_concurrency - 1，调用线程算 thread 0）。
    // 刻意不自己算那个数：那是重复它的策略，它哪天改了我们就悄悄漂了。
    uint32_t worker_count{0};

    // 将来要调 registerExternalThread() 的线程数（渲染线程，或者调用方自己起的线程）。
    // enkiTS 必须在 Initialize 时就知道这个数，事后补不上。
    uint32_t external_thread_count{0};

    // 给 worker 起名 ArtiChoco-Worker-<n>。调试器的线程窗口和 profiler 靠它认人 ——
    // 这是「活到底有没有真的分出去」最便宜的观测手段。
    bool name_threads{true};
};

// 异步任务的句柄：槽位下标 + 世代号。槽位被回收再利用时世代号 +1，所以**陈旧句柄是安全的**
// —— 世代号不匹配就当「那个任务早完事了」，wait() 是空操作、isComplete() 返回 true。
// 32 位世代号，回绕不现实。
struct TaskHandle {
    static constexpr uint32_t kInvalidIndex{0xFFFF'FFFFu};

    uint32_t index{kInvalidIndex};
    uint32_t generation{0};

    bool valid() const noexcept
    {
        return index != kInvalidIndex;
    }
};

// enkiTS 有五档，这里只暴露三档。中间那两档（MED_HI / MED_LO）现在没有能说清楚的用途，
// 暴露出来只会让调用方在「到底该填哪个」上纠结。
//
// **留着没做的那一半**：优先级真正生效靠 enkiTS 的 WaitforTask(task, priorityOfLowestToRun_)
// —— 高优先级的等待不会去跑低优先级的活。等真有「帧内 / 后台流式」之分的消费者时再加。
enum class TaskPriority : uint8_t {
    High,
    Normal,
    Low
};

struct ParallelForOptions {
    // enkiTS 的 grain size（m_MinRange）。默认 1 是为了跟老行为一致，但**每个分片的活少于
    // 大约一万个时钟周期就该往上调** —— 否则调度开销能把并行的收益吃光。
    uint32_t min_range{1};

    TaskPriority priority{TaskPriority::Normal};
};

// 分片回调：[begin, end) 加当前线程下标。thread_index 的用途就是「每线程一个输出 bucket」，
// 不要拿它去改变处理逻辑。
using ParallelForRangeFunction = std::function<void(uint32_t begin, uint32_t end, uint32_t thread_index)>;

// enkiTS 的封装。**进程级**单例，生命周期由 init() / shutdown() 显式管，和 Logger 一样 ——
// asset_tools 那样的 CLI 没有 Application，绑在 Application 上的东西它根本拿不到。
class TaskSystem {
public:
    TaskSystem(const TaskSystem&) = delete;
    TaskSystem& operator=(const TaskSystem&) = delete;

    // 重复 init 是空操作（记一条 warn）。线程池是进程级资源，在有任务在跑的时候把它拆了重建，
    // 比忽略一次多余的 init 危险得多。
    static void init(const TaskSystemConfig& config = {});
    static void shutdown();
    static bool isInitialized();

    // 未 init 时抛 std::logic_error —— 比让调用方拿一个空引用再解引用更早也更明确。
    static TaskSystem& get();

    // 阻塞的 fork-join。任务对象在栈上、生命周期由这个函数框住，所以不进池。
    template <typename Fn>
    void parallelFor(uint32_t count, Fn&& function, ParallelForOptions options = {})
    {
        parallelForRangesImpl(
                count,
                [fn = std::forward<Fn>(function)](uint32_t begin, uint32_t end, uint32_t) mutable {
                    for (uint32_t index = begin; index < end; ++index) {
                        fn(index);
                    }
                },
                options);
    }

    // 按分片的版本。逐元素那个是这个的糖 —— 每线程一个 bucket 的写法要用这个。
    template <typename Fn>
    void parallelForRanges(uint32_t count, Fn&& function, ParallelForOptions options = {})
    {
        parallelForRangesImpl(count, std::forward<Fn>(function), options);
    }

    // 异步。返回的句柄可以 wait() / isComplete()，也可以直接丢掉（任务照跑，完成后自动回收）。
    template <typename Fn>
    TaskHandle submit(Fn&& function, TaskPriority priority = TaskPriority::Normal)
    {
        return submitImpl([fn = std::forward<Fn>(function)]() mutable { fn(); }, priority);
    }

    // 异步的 parallel-for。Box3D 的任务回调要的就是这个形状（拿一个可等待的东西回来）。
    template <typename Fn>
    TaskHandle submitParallelFor(uint32_t count, Fn&& function, ParallelForOptions options = {})
    {
        return submitParallelForRangesImpl(
                count,
                [fn = std::forward<Fn>(function)](uint32_t begin, uint32_t end, uint32_t) mutable {
                    for (uint32_t index = begin; index < end; ++index) {
                        fn(index);
                    }
                },
                options);
    }

    template <typename Fn>
    TaskHandle submitParallelForRanges(
            uint32_t count, Fn&& function, ParallelForOptions options = {})
    {
        return submitParallelForRangesImpl(count, std::forward<Fn>(function), options);
    }

    // 钉在指定 enkiTS 线程上跑。thread_index 必须 < threadCount()，否则抛 logic_error。
    // 长驻循环（渲染线程那种）就是 submitPinned 之后一直不 wait，等退出时再 wait。
    template <typename Fn>
    TaskHandle submitPinned(uint32_t thread_index, Fn&& function)
    {
        return submitPinnedImpl(
                thread_index, [fn = std::forward<Fn>(function)]() mutable { fn(); });
    }

    // 当前线程把自己 pinned 队列排空。WaitforTask 在等待期间会顺手跑本线程的 pinned，
    // 所以普通 wait() 不需要先调这个；需要的是「我这条线程只吃 pinned、自己泵队列」那种循环。
    void runPinnedTasks();

    // 陈旧句柄安全：指向已回收槽位的句柄当「早完事了」处理，不崩、不阻塞。
    void wait(TaskHandle handle);
    bool isComplete(TaskHandle handle) const;

    // **屏障 / 关停用，不是通用同步手段。** enkiTS 自己的注释就写着：在任务被持续加入的情况下
    // 它不保证有效。要等「我关心的那批活」，用句柄。
    void waitForAll();

    // 未注册进调度器的线程（普通 std::thread、还没 registerExternalThread 的渲染线程）。
    // 值和 enkiTS 的 NO_THREAD_NUM 一样，故意不 include 它的头。
    static constexpr uint32_t kNoThread{0xFFFF'FFFFu};

    // GetThreadNum()。调用线程是 0；worker / 已注册的外部线程是 [1, threadCount())；
    // 没注册的返回 kNoThread。
    uint32_t threadIndex() const noexcept;

    // 含调用线程。等于 worker_count + external_thread_count + 1。
    uint32_t threadCount() const noexcept;

    // enkiTS 自己建的 worker 数，不含调用线程、不含外部线程。
    // 不是 threadCount()-1：有 external_thread_count 时那个公式会把外部槽位算进去。
    uint32_t workerCount() const noexcept;

    // 当前线程参与调度。必须在 init 时就把 external_thread_count 留够，事后补不上。
    // 返回 false = 槽位满了或没留。
    bool registerExternalThread();
    void deregisterExternalThread();

    // 诊断用：池子里一共开了多少槽位。测试靠它证明回收真的在工作（submit 十万次之后这个数
    // 应该有上界，而不是跟着次数长）。
    std::size_t taskSlotCount() const;

private:
    explicit TaskSystem(const TaskSystemConfig& config);
    ~TaskSystem();

    void parallelForRangesImpl(
            uint32_t count, const ParallelForRangeFunction& function, ParallelForOptions options);
    TaskHandle submitParallelForRangesImpl(
            uint32_t count, ParallelForRangeFunction function, ParallelForOptions options);
    TaskHandle submitImpl(std::function<void()> function, TaskPriority priority);
    TaskHandle submitPinnedImpl(uint32_t thread_index, std::function<void()> function);

    // 占槽位 → 入队 → markLaunched。这个顺序是固定的，理由在 task_pool.h：任务在入队之前
    // GetIsComplete() 就是 true，回收扫描必须先看到「还没上线」这个状态。
    TaskHandle launch(std::shared_ptr<enki::TaskSet> task);
    TaskHandle launchPinned(std::shared_ptr<enki::LambdaPinnedTask> task);

    std::unique_ptr<enki::TaskScheduler> m_scheduler;
    std::unique_ptr<detail::TaskPool> m_pool;
};

} // namespace arti::core
