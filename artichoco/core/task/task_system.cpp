#include "task_system.h"

#include "log.h"
#include "task_pool.h"
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

enki::TaskPriority toEnkiPriority(TaskPriority priority)
{
    switch (priority) {
    case TaskPriority::High:
        return enki::TASK_PRIORITY_HIGH;
    case TaskPriority::Low:
        return enki::TASK_PRIORITY_LOW;
    case TaskPriority::Normal:
        break;
    }

    return enki::TASK_PRIORITY_MED;
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
            s_instance->threadCount());
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
    , m_pool(std::make_unique<detail::TaskPool>())
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

void TaskSystem::parallelForRangesImpl(
        uint32_t count, const ParallelForRangeFunction& function, ParallelForOptions options)
{
    enki::TaskSet task{count,
            [&function](enki::TaskSetPartition range, uint32_t thread_index) {
                function(range.start, range.end, thread_index);
            }};
    task.m_MinRange = options.min_range > 0 ? options.min_range : 1;
    task.m_Priority = toEnkiPriority(options.priority);

    m_scheduler->AddTaskSetToPipe(&task);
    m_scheduler->WaitforTask(&task);
}

TaskHandle TaskSystem::submitParallelForRangesImpl(
        uint32_t count, ParallelForRangeFunction function, ParallelForOptions options)
{
    auto task = std::make_shared<enki::TaskSet>(count,
            [fn = std::move(function)](enki::TaskSetPartition range, uint32_t thread_index) {
                fn(range.start, range.end, thread_index);
            });
    task->m_MinRange = options.min_range > 0 ? options.min_range : 1;
    task->m_Priority = toEnkiPriority(options.priority);

    return launch(std::move(task));
}

TaskHandle TaskSystem::submitImpl(std::function<void()> function, TaskPriority priority)
{
    auto task = std::make_shared<enki::TaskSet>(
            [fn = std::move(function)](enki::TaskSetPartition, uint32_t) { fn(); });
    task->m_Priority = toEnkiPriority(priority);

    return launch(std::move(task));
}

TaskHandle TaskSystem::submitPinnedImpl(uint32_t thread_index, std::function<void()> function)
{
    if (thread_index >= threadCount()) {
        throw std::logic_error("TaskSystem::submitPinned thread_index is out of range.");
    }

    auto task = std::make_shared<enki::LambdaPinnedTask>(thread_index, std::move(function));
    return launchPinned(std::move(task));
}

void TaskSystem::runPinnedTasks()
{
    m_scheduler->RunPinnedTasks();
}

TaskHandle TaskSystem::launch(std::shared_ptr<enki::TaskSet> task)
{
    // 顺序是有讲究的：先占槽位（此时是 pending，回收扫描会跳过它），再入队，最后翻成
    // launched。反过来先入队的话，任务可能在拿到句柄之前就跑完并被回收掉。
    const TaskHandle handle = m_pool->insert(task);
    m_scheduler->AddTaskSetToPipe(task.get());
    m_pool->markLaunched(handle);

    return handle;
}

TaskHandle TaskSystem::launchPinned(std::shared_ptr<enki::LambdaPinnedTask> task)
{
    const TaskHandle handle = m_pool->insert(task);
    m_scheduler->AddPinnedTask(task.get());
    m_pool->markLaunched(handle);

    return handle;
}

void TaskSystem::wait(TaskHandle handle)
{
    // **先把 shared_ptr 拷出来再等。** find() 自己进出锁，所以调 WaitforTask 时我们不持任何锁
    // —— 它会在等待期间跑别的任务，那些任务可能又来 submit，持着池子的锁进去就是死锁。
    const auto task = m_pool->find(handle);
    if (task) {
        m_scheduler->WaitforTask(task.get());
    }
}

bool TaskSystem::isComplete(TaskHandle handle) const
{
    const auto task = m_pool->find(handle);
    // 找不到 = 槽位已被回收 = 那个任务早完事了。
    return !task || task->GetIsComplete();
}

uint32_t TaskSystem::threadIndex() const noexcept
{
    return m_scheduler->GetThreadNum();
}

uint32_t TaskSystem::threadCount() const noexcept
{
    return m_scheduler->GetNumTaskThreads();
}

uint32_t TaskSystem::workerCount() const noexcept
{
    return m_scheduler->GetConfig().numTaskThreadsToCreate;
}

bool TaskSystem::registerExternalThread()
{
    return m_scheduler->RegisterExternalTaskThread();
}

void TaskSystem::deregisterExternalThread()
{
    m_scheduler->DeRegisterExternalTaskThread();
}

std::size_t TaskSystem::taskSlotCount() const
{
    return m_pool->slotCount();
}

void TaskSystem::waitForAll()
{
    m_scheduler->WaitforAll();
    m_pool->reclaimCompleted();
}

} // namespace arti::core
