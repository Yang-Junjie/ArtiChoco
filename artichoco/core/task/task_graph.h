#pragma once

#include "task/task_system.h"

#include <cstdint>

#include <functional>
#include <initializer_list>
#include <memory>
#include <span>

namespace enki {
class ICompletable;
class TaskScheduler;
} // namespace enki

namespace arti::core {

namespace detail {
class TaskPool;
} // namespace detail

// 图内节点标识。不是 TaskHandle —— 图还没提交，两种 ID 长得一样只会让人把还没入队的
// 节点拿去 wait()。提交之后等的是整张图那个句柄。
struct TaskGraphNode {
    uint32_t index{0xFFFF'FFFFu};
};

// 先建图、后提交。enkiTS 的依赖边必须在前驱入队之前连好，对已经在跑的句柄调
// SetDependency 会让后继永远不启动（D4）。所以没有 then(runningHandle)。
class TaskGraph {
public:
    TaskGraph();
    TaskGraph(TaskGraph&& other) noexcept;
    TaskGraph& operator=(TaskGraph&& other) noexcept;
    ~TaskGraph();

    TaskGraph(const TaskGraph&) = delete;
    TaskGraph& operator=(const TaskGraph&) = delete;

    template <typename Fn>
    TaskGraphNode add(Fn&& function, TaskPriority priority = TaskPriority::Normal)
    {
        return addImpl([fn = std::forward<Fn>(function)]() mutable { fn(); }, priority, {});
    }

    template <typename Fn>
    TaskGraphNode addAfter(
            std::initializer_list<TaskGraphNode> predecessors,
            Fn&& function,
            TaskPriority priority = TaskPriority::Normal)
    {
        return addImpl(
                [fn = std::forward<Fn>(function)]() mutable { fn(); },
                priority,
                std::span<const TaskGraphNode>{predecessors.begin(), predecessors.size()});
    }

    template <typename Fn>
    TaskGraphNode addParallelFor(
            uint32_t count, Fn&& function, ParallelForOptions options = {})
    {
        return addParallelForImpl(
                count,
                [fn = std::forward<Fn>(function)](uint32_t begin, uint32_t end, uint32_t) mutable {
                    for (uint32_t index = begin; index < end; ++index) {
                        fn(index);
                    }
                },
                options,
                {});
    }

    template <typename Fn>
    TaskGraphNode addParallelForAfter(
            std::initializer_list<TaskGraphNode> predecessors,
            uint32_t count,
            Fn&& function,
            ParallelForOptions options = {})
    {
        return addParallelForImpl(
                count,
                [fn = std::forward<Fn>(function)](uint32_t begin, uint32_t end, uint32_t) mutable {
                    for (uint32_t index = begin; index < end; ++index) {
                        fn(index);
                    }
                },
                options,
                std::span<const TaskGraphNode>{predecessors.begin(), predecessors.size()});
    }

    template <typename Fn>
    TaskGraphNode addPinned(uint32_t thread_index, Fn&& function)
    {
        return addPinnedImpl(
                thread_index, [fn = std::forward<Fn>(function)]() mutable { fn(); }, {});
    }

    template <typename Fn>
    TaskGraphNode addPinnedAfter(
            std::initializer_list<TaskGraphNode> predecessors,
            uint32_t thread_index,
            Fn&& function)
    {
        return addPinnedImpl(
                thread_index,
                [fn = std::forward<Fn>(function)]() mutable { fn(); },
                std::span<const TaskGraphNode>{predecessors.begin(), predecessors.size()});
    }

private:
    friend class TaskSystem;

    struct Storage;

    TaskGraphNode addImpl(
            std::function<void()> function,
            TaskPriority priority,
            std::span<const TaskGraphNode> predecessors);
    TaskGraphNode addParallelForImpl(
            uint32_t count,
            ParallelForRangeFunction function,
            ParallelForOptions options,
            std::span<const TaskGraphNode> predecessors);
    TaskGraphNode addPinnedImpl(
            uint32_t thread_index,
            std::function<void()> function,
            std::span<const TaskGraphNode> predecessors);
    void linkPredecessors(
            TaskGraphNode node, std::span<const TaskGraphNode> predecessors);

    TaskHandle submitTo(enki::TaskScheduler& scheduler, detail::TaskPool& pool);

    std::shared_ptr<Storage> m_storage;
};

} // namespace arti::core
