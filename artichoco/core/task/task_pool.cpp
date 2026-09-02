#include "task/task_pool.h"

#include "TaskScheduler.h"

#include <utility>

namespace arti::core::detail {

TaskHandle TaskPool::insert(TaskPtr task)
{
    std::lock_guard lock{m_mutex};

    // 只在空闲表空了才扫一遍：有空闲槽位时 insert 是 O(1)，扫描摊到「用光一轮」上。
    // 每次 insert 都全扫的话，在途任务多的时候是 O(n²)。
    if (m_free.empty()) {
        reclaimCompletedLocked();
    }
    if (m_free.empty()) {
        m_slots.emplace_back();
        m_free.push_back(static_cast<uint32_t>(m_slots.size() - 1));
    }

    const uint32_t index = m_free.back();
    m_free.pop_back();

    Slot& slot = m_slots[index];
    slot.task = std::move(task);
    slot.launched = false;

    return TaskHandle{index, slot.generation};
}

void TaskPool::markLaunched(TaskHandle handle)
{
    std::lock_guard lock{m_mutex};

    if (handle.index >= m_slots.size()) {
        return;
    }

    Slot& slot = m_slots[handle.index];
    if (slot.generation == handle.generation) {
        slot.launched = true;
    }
}

TaskPool::TaskPtr TaskPool::find(TaskHandle handle) const
{
    std::lock_guard lock{m_mutex};

    if (handle.index >= m_slots.size()) {
        return nullptr;
    }

    const Slot& slot = m_slots[handle.index];
    if (slot.generation != handle.generation) {
        // 世代号不匹配 = 槽位早被回收再利用了，也就是那个任务早完事了。
        return nullptr;
    }

    return slot.task;
}

void TaskPool::reclaimCompleted()
{
    std::lock_guard lock{m_mutex};
    reclaimCompletedLocked();
}

std::size_t TaskPool::slotCount() const
{
    std::lock_guard lock{m_mutex};
    return m_slots.size();
}

void TaskPool::reclaimCompletedLocked()
{
    for (uint32_t index = 0; index < static_cast<uint32_t>(m_slots.size()); ++index) {
        Slot& slot = m_slots[index];
        // launched 那一半见头文件：没入队的任务 GetIsComplete() 也是 true。
        if (slot.task && slot.launched && slot.task->GetIsComplete()) {
            slot.task.reset();
            slot.launched = false;
            ++slot.generation;
            m_free.push_back(index);
        }
    }
}

} // namespace arti::core::detail
