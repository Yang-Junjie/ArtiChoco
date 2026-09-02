#pragma once

#include "task/task_system.h"

#include <cstddef>
#include <cstdint>

#include <memory>
#include <mutex>
#include <vector>

namespace enki {
class ICompletable;
} // namespace enki

namespace arti::core::detail {

// 在途任务的存放处。只被 TaskSystem 用，不进公开 API。
//
// **任务对象存成 shared_ptr 而不是 unique_ptr**：wait() 必须在**不持锁**的情况下调 enkiTS 的
// WaitforTask（它会在等待期间跑别的任务，那些任务可能又来 submit —— 持锁进去就是死锁）。
// 持锁期间把 shared_ptr 拷一份出来再解锁，槽位就算同时被别人回收，对象也活到等待者用完为止。
// 代价是每个任务一个控制块；真嫌重的话接缝是换个池分配器，但那要先有 profiler 数据。
class TaskPool {
public:
    using TaskPtr = std::shared_ptr<enki::ICompletable>;

    // 占一个槽位。此时槽位是 **pending**：enkiTS 要到 AddTaskSetToPipe 才把 m_RunningCount
    // 抬起来，所以在入队之前 GetIsComplete() 是 true —— 回收扫描必须跳过 pending 的槽位，
    // 否则任务还没入队就被当成「已完成」回收掉了。
    TaskHandle insert(TaskPtr task);

    // 入队之后调，把槽位从 pending 翻成可回收。
    void markLaunched(TaskHandle handle);

    // 句柄对应的任务对象。返回空表示「这个句柄指的任务早完事并且槽位已经被回收了」。
    TaskPtr find(TaskHandle handle) const;

    // 已完成的槽位还给空闲表、世代号 +1。
    void reclaimCompleted();

    std::size_t slotCount() const;

private:
    struct Slot {
        TaskPtr task;
        uint32_t generation{0};
        bool launched{false};
    };

    void reclaimCompletedLocked();

    mutable std::mutex m_mutex;
    std::vector<Slot> m_slots;
    std::vector<uint32_t> m_free;
};

} // namespace arti::core::detail
