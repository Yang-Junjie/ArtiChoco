#pragma once

#include <cstdint>

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace enki {
class TaskScheduler;
class TaskSet;
class LambdaPinnedTask;
} // namespace enki

namespace arti::core {

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

    template <typename Fn>
    void parallelFor(uint32_t count, Fn&& function)
    {
        parallelForImpl(count, [fn = std::forward<Fn>(function)](uint32_t index) mutable { fn(index); });
    }

    template <typename Fn>
    void submit(Fn&& function)
    {
        submitImpl([fn = std::forward<Fn>(function)]() mutable { fn(); });
    }

    template <typename Fn>
    void pinned(uint32_t thread_index, Fn&& function)
    {
        pinnedImpl(thread_index, [fn = std::forward<Fn>(function)]() mutable { fn(); });
    }

    void launchPinned(uint32_t thread_index, const std::function<void()>& function);
    void waitForPinnedTask();

    // 注意：含调用线程。等于 worker_count + external_thread_count + 1。
    uint32_t taskThreadCount() const noexcept;

    void waitForAll();

private:
    explicit TaskSystem(const TaskSystemConfig& config);
    ~TaskSystem();

    void parallelForImpl(uint32_t count, const std::function<void(uint32_t)>& function);
    void submitImpl(const std::function<void()>& function);
    void pinnedImpl(uint32_t thread_index, const std::function<void()>& function);

    std::unique_ptr<enki::TaskScheduler> m_scheduler;
    std::mutex m_pending_mutex;
    std::vector<std::unique_ptr<enki::TaskSet>> m_pending;
    std::unique_ptr<enki::LambdaPinnedTask> m_pinned_task;
};

} // namespace arti::core
