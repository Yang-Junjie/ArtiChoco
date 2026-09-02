#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <memory>

namespace enki {
class TaskScheduler;
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
    void parallelFor(uint32_t count, Fn&& function)
    {
        parallelForImpl(count, [fn = std::forward<Fn>(function)](uint32_t index) mutable { fn(index); });
    }

    // 异步。返回的句柄可以 wait() / isComplete()，也可以直接丢掉（任务照跑，完成后自动回收）。
    template <typename Fn>
    TaskHandle submit(Fn&& function)
    {
        return submitImpl([fn = std::forward<Fn>(function)]() mutable { fn(); });
    }

    template <typename Fn>
    void pinned(uint32_t thread_index, Fn&& function)
    {
        pinnedImpl(thread_index, [fn = std::forward<Fn>(function)]() mutable { fn(); });
    }

    void launchPinned(uint32_t thread_index, const std::function<void()>& function);
    void waitForPinnedTask();

    // 陈旧句柄安全：指向已回收槽位的句柄当「早完事了」处理，不崩、不阻塞。
    void wait(TaskHandle handle);
    bool isComplete(TaskHandle handle) const;

    // **屏障 / 关停用，不是通用同步手段。** enkiTS 自己的注释就写着：在任务被持续加入的情况下
    // 它不保证有效。要等「我关心的那批活」，用句柄。
    void waitForAll();

    // 注意：含调用线程。等于 worker_count + external_thread_count + 1。
    uint32_t taskThreadCount() const noexcept;

    // 诊断用：池子里一共开了多少槽位。测试靠它证明回收真的在工作（submit 十万次之后这个数
    // 应该有上界，而不是跟着次数长）。
    std::size_t taskSlotCount() const;

private:
    explicit TaskSystem(const TaskSystemConfig& config);
    ~TaskSystem();

    void parallelForImpl(uint32_t count, const std::function<void(uint32_t)>& function);
    TaskHandle submitImpl(std::function<void()> function);
    void pinnedImpl(uint32_t thread_index, const std::function<void()>& function);

    std::unique_ptr<enki::TaskScheduler> m_scheduler;
    std::unique_ptr<detail::TaskPool> m_pool;
    std::unique_ptr<enki::LambdaPinnedTask> m_pinned_task;
};

} // namespace arti::core
