#include "task_system.h"

#include "TaskScheduler.h"

#include <utility>

namespace arti::core {
namespace {
TaskSystem* s_instance = nullptr;
}

TaskSystem::TaskSystem()
    : m_scheduler(std::make_unique<enki::TaskScheduler>())
{
    m_scheduler->Initialize();
    s_instance = this;
}

TaskSystem::~TaskSystem()
{
    m_scheduler->WaitforAllAndShutdown();
    s_instance = nullptr;
}

TaskSystem& TaskSystem::get()
{
    return *s_instance;
}

void TaskSystem::parallelForImpl(uint32_t count, const std::function<void(uint32_t)>& function)
{
    enki::TaskSet task{count, [&function](enki::TaskSetPartition range, uint32_t) {
                          for (uint32_t index = range.start; index < range.end; ++index) {
                              function(index);
                          }
                      }};
    m_scheduler->AddTaskSetToPipe(&task);
    m_scheduler->WaitforTask(&task);
}

void TaskSystem::submitImpl(const std::function<void()>& function)
{
    auto task = std::make_unique<enki::TaskSet>(
        [function](enki::TaskSetPartition, uint32_t) { function(); });
    enki::TaskSet* raw = task.get();
    {
        std::lock_guard lock{m_pending_mutex};
        m_pending.push_back(std::move(task));
    }
    m_scheduler->AddTaskSetToPipe(raw);
}

void TaskSystem::pinnedImpl(uint32_t thread_index, const std::function<void()>& function)
{
    enki::LambdaPinnedTask task{thread_index, function};
    m_scheduler->AddPinnedTask(&task);
    m_scheduler->WaitforTask(&task);
    m_scheduler->RunPinnedTasks();
}

void TaskSystem::launchPinned(uint32_t thread_index, const std::function<void()>& function)
{
    m_pinned_task = std::make_unique<enki::LambdaPinnedTask>(thread_index, function);
    m_scheduler->AddPinnedTask(m_pinned_task.get());
}

void TaskSystem::waitForPinnedTask()
{
    if (m_pinned_task) {
        m_scheduler->WaitforTask(m_pinned_task.get());
    }
}

uint32_t TaskSystem::taskThreadCount() const noexcept
{
    return m_scheduler->GetNumTaskThreads();
}

void TaskSystem::waitForAll()
{
    std::vector<std::unique_ptr<enki::TaskSet>> pending;
    {
        std::lock_guard lock{m_pending_mutex};
        pending.swap(m_pending);
    }
    for (const auto& task : pending) {
        m_scheduler->WaitforTask(task.get());
    }
}

} // namespace arti::core
