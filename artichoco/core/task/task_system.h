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

class TaskSystem {
public:
    TaskSystem();
    ~TaskSystem();

    TaskSystem(const TaskSystem&) = delete;
    TaskSystem& operator=(const TaskSystem&) = delete;

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
    uint32_t taskThreadCount() const noexcept;

    void waitForAll();

private:
    void parallelForImpl(uint32_t count, const std::function<void(uint32_t)>& function);
    void submitImpl(const std::function<void()>& function);
    void pinnedImpl(uint32_t thread_index, const std::function<void()>& function);

    std::unique_ptr<enki::TaskScheduler> m_scheduler;
    std::mutex m_pending_mutex;
    std::vector<std::unique_ptr<enki::TaskSet>> m_pending;
    std::unique_ptr<enki::LambdaPinnedTask> m_pinned_task;
};

} // namespace arti::core
