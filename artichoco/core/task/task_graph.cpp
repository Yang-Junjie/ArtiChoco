#include "task_graph.h"

#include "task_pool.h"

#include "TaskScheduler.h"

#include <stdexcept>
#include <utility>
#include <vector>

namespace arti::core {
namespace {

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

struct TaskGraph::Storage : enki::ICompletable {
    enum class Kind : uint8_t {
        TaskSet,
        Pinned
    };

    struct Node {
        std::shared_ptr<enki::ICompletable> task;
        std::vector<enki::Dependency> incoming;
        uint32_t successor_count{0};
        Kind kind{Kind::TaskSet};
    };

    std::vector<Node> nodes;
    std::vector<enki::Dependency> terminator_incoming;
};

TaskGraph::TaskGraph()
    : m_storage(std::make_shared<Storage>())
{
}

TaskGraph::TaskGraph(TaskGraph&& other) noexcept = default;
TaskGraph& TaskGraph::operator=(TaskGraph&& other) noexcept = default;
TaskGraph::~TaskGraph() = default;

TaskGraphNode TaskGraph::addImpl(
        std::function<void()> function,
        TaskPriority priority,
        std::span<const TaskGraphNode> predecessors)
{
    if (!m_storage) {
        throw std::logic_error("TaskGraph::add called on a moved-from graph.");
    }

    auto task = std::make_shared<enki::TaskSet>(
            [fn = std::move(function)](enki::TaskSetPartition, uint32_t) { fn(); });
    task->m_Priority = toEnkiPriority(priority);

    Storage::Node node;
    node.task = std::move(task);
    node.kind = Storage::Kind::TaskSet;

    const TaskGraphNode id{static_cast<uint32_t>(m_storage->nodes.size())};
    m_storage->nodes.push_back(std::move(node));
    linkPredecessors(id, predecessors);
    return id;
}

TaskGraphNode TaskGraph::addParallelForImpl(
        uint32_t count,
        ParallelForRangeFunction function,
        ParallelForOptions options,
        std::span<const TaskGraphNode> predecessors)
{
    if (!m_storage) {
        throw std::logic_error("TaskGraph::addParallelFor called on a moved-from graph.");
    }

    auto task = std::make_shared<enki::TaskSet>(count,
            [fn = std::move(function)](enki::TaskSetPartition range, uint32_t thread_index) {
                fn(range.start, range.end, thread_index);
            });
    task->m_MinRange = options.min_range > 0 ? options.min_range : 1;
    task->m_Priority = toEnkiPriority(options.priority);

    Storage::Node node;
    node.task = std::move(task);
    node.kind = Storage::Kind::TaskSet;

    const TaskGraphNode id{static_cast<uint32_t>(m_storage->nodes.size())};
    m_storage->nodes.push_back(std::move(node));
    linkPredecessors(id, predecessors);
    return id;
}

TaskGraphNode TaskGraph::addPinnedImpl(
        uint32_t thread_index,
        std::function<void()> function,
        std::span<const TaskGraphNode> predecessors)
{
    if (!m_storage) {
        throw std::logic_error("TaskGraph::addPinned called on a moved-from graph.");
    }

    auto task = std::make_shared<enki::LambdaPinnedTask>(thread_index, std::move(function));

    Storage::Node node;
    node.task = std::move(task);
    node.kind = Storage::Kind::Pinned;

    const TaskGraphNode id{static_cast<uint32_t>(m_storage->nodes.size())};
    m_storage->nodes.push_back(std::move(node));
    linkPredecessors(id, predecessors);
    return id;
}

void TaskGraph::linkPredecessors(
        TaskGraphNode node, std::span<const TaskGraphNode> predecessors)
{
    Storage::Node& stored = m_storage->nodes[node.index];
    stored.incoming.reserve(predecessors.size());

    for (const TaskGraphNode pred: predecessors) {
        if (pred.index >= node.index) {
            throw std::logic_error(
                    "TaskGraph predecessor is invalid or points at a node that does not exist yet.");
        }

        Storage::Node& pred_node = m_storage->nodes[pred.index];
        ++pred_node.successor_count;

        stored.incoming.emplace_back();
        stored.incoming.back().SetDependency(pred_node.task.get(), stored.task.get());
    }
}

TaskHandle TaskGraph::submitTo(enki::TaskScheduler& scheduler, detail::TaskPool& pool)
{
    if (!m_storage) {
        throw std::logic_error("TaskGraph::submit called on a moved-from graph.");
    }

    auto storage = std::move(m_storage);
    const uint32_t thread_count = scheduler.GetNumTaskThreads();

    uint32_t root_count = 0;
    for (Storage::Node& node: storage->nodes) {
        if (node.kind == Storage::Kind::Pinned) {
            const auto* pinned = static_cast<enki::IPinnedTask*>(node.task.get());
            if (pinned->threadNum >= thread_count) {
                throw std::logic_error("TaskGraph pinned node thread_index is out of range.");
            }
        }
        if (node.incoming.empty()) {
            ++root_count;
        }
    }

    if (!storage->nodes.empty() && root_count == 0) {
        throw std::logic_error("TaskGraph has no root nodes; the graph is cyclic or closed.");
    }

    // 终结节点依赖所有叶子。等它 = 等整张图，调用方不用自己收集出度为 0 的节点。
    for (Storage::Node& node: storage->nodes) {
        if (node.successor_count == 0) {
            storage->terminator_incoming.emplace_back();
            storage->terminator_incoming.back().SetDependency(node.task.get(), storage.get());
        }
    }

    const TaskHandle handle = pool.insert(storage);
    for (Storage::Node& node: storage->nodes) {
        if (!node.incoming.empty()) {
            continue;
        }
        if (node.kind == Storage::Kind::Pinned) {
            scheduler.AddPinnedTask(static_cast<enki::IPinnedTask*>(node.task.get()));
        } else {
            scheduler.AddTaskSetToPipe(static_cast<enki::ITaskSet*>(node.task.get()));
        }
    }
    pool.markLaunched(handle);
    return handle;
}

} // namespace arti::core
