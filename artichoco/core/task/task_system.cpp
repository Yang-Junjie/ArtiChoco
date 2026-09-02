#include "task_system.h"

#include "log.h"
#include "thread_naming.h"

#include "TaskScheduler.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace arti::core {
namespace {

TaskSystem* s_instance = nullptr;

// enkiTS 的 profiler 回调是**裸函数指针、没有 userData**（TaskScheduler.h 的 ProfilerCallbacks），
// 所以「要不要起名」这个开关只能放在文件作用域里让回调看得见。
bool s_name_threads = false;

// 只有 enkiTS 自己建的 worker 会走到这里 —— 调用线程（thread 0）不是它建的，
// 所以主线程的名字不会被我们改掉。
void onTaskThreadStart(uint32_t thread_index)
{
    if (s_name_threads) {
        detail::setCurrentThreadName(("ArtiChoco-Worker-" + std::to_string(thread_index)).c_str());
    }
}

} // namespace

void TaskSystem::init(const TaskSystemConfig& config)
{
    if (s_instance != nullptr) {
        ARTI_CORE_WARN("TaskSystem::init called twice; keeping the existing scheduler");
        return;
    }

    s_instance = new TaskSystem(config);
    ARTI_CORE_INFO("TaskSystem initialized: {} thread(s) including the calling thread",
            s_instance->taskThreadCount());
}

void TaskSystem::shutdown()
{
    if (s_instance == nullptr) {
        return;
    }

    delete s_instance;
    s_instance = nullptr;
    ARTI_CORE_INFO("TaskSystem shutdown finished");
}

bool TaskSystem::isInitialized()
{
    return s_instance != nullptr;
}

TaskSystem& TaskSystem::get()
{
    if (s_instance == nullptr) {
        throw std::logic_error("TaskSystem::get() called before TaskSystem::init().");
    }

    return *s_instance;
}

TaskSystem::TaskSystem(const TaskSystemConfig& config)
    : m_scheduler(std::make_unique<enki::TaskScheduler>())
{
    // 线程一开起来就会调 onTaskThreadStart，所以这个开关必须在 Initialize 之前落地。
    s_name_threads = config.name_threads;

    // 从 enkiTS 自己的默认出发，只改真正想改的字段 —— worker_count == 0 时
    // numTaskThreadsToCreate 保持它算好的 hardware_concurrency - 1。
    enki::TaskSchedulerConfig enki_config = m_scheduler->GetConfig();
    if (config.worker_count > 0) {
        enki_config.numTaskThreadsToCreate = config.worker_count;
    }
    enki_config.numExternalTaskThreads = config.external_thread_count;
    enki_config.profilerCallbacks.threadStart = &onTaskThreadStart;
    // 其余七个回调留空（enkiTS 内部对 nullptr 有判断，见它的 SafeCallback）。将来接 profiler
    // 就填这里：threadStop、waitForNewTaskSuspendStart/Stop（worker 空转挂起）、
    // waitForTaskCompleteStart/Stop 和 waitForTaskCompleteSuspendStart/Stop（等任务完成）。

    m_scheduler->Initialize(enki_config);
}

TaskSystem::~TaskSystem()
{
    m_scheduler->WaitforAllAndShutdown();
    s_name_threads = false;
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
