#include "scene_hierarchy.h"

#include "scene_log.h"

#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace arti::scene {
namespace {

bool matricesEqual(const glm::mat4& left, const glm::mat4& right) noexcept {
    for (glm::length_t column = 0; column < 4; ++column) {
        for (glm::length_t row = 0; row < 4; ++row) {
            if (left[column][row] != right[column][row]) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

SceneHierarchy::SceneHierarchy(SceneEntityStorage& storage) noexcept
        : m_storage(storage) {}

void SceneHierarchy::setParent(Entity child, Entity parent) {
    if (!m_storage.isValid(child) || !m_storage.isValid(parent)) {
        getLogChannel().warn(
                "Rejected parent assignment: an Entity is invalid or belongs to another Scene.");
        throw std::invalid_argument(
                "The Entity does not belong to this Scene or is no longer valid.");
    }
    if (child.m_handle == parent.m_handle) {
        getLogChannel().warn("Rejected parent assignment: an Entity cannot be its own parent.");
        throw std::invalid_argument("An Entity cannot be its own parent.");
    }

    auto& registry = m_storage.registry();
    for (entt::entity cursor = parent.m_handle; cursor != entt::null;) {
        if (cursor == child.m_handle) {
            getLogChannel().warn(
                    "Rejected parent assignment: the relationship would create a cycle.");
            throw std::invalid_argument("The requested parent relationship would create a cycle.");
        }
        const auto& parent_component = registry.get<ParentComponent>(cursor);
        if (!parent_component.parent_id.isValid()) {
            break;
        }
        cursor = m_storage.resolveEntity(parent_component.parent_id);
    }

    registry.get<ParentComponent>(child.m_handle).parent_id = parent.getUUID();
    registry.get<WorldTransformComponent>(child.m_handle).dirty = true;
    updateWorldTransform(child.m_handle);
    getLogChannel().debug("Set parent of entity {} to {}",
            child.getUUID().toString(), parent.getUUID().toString());
}

void SceneHierarchy::detachFromParent(Entity entity) {
    if (!m_storage.isValid(entity)) {
        getLogChannel().warn(
                "Rejected parent detachment: the Entity is invalid or belongs to another Scene.");
        throw std::invalid_argument(
                "The Entity does not belong to this Scene or is no longer valid.");
    }

    auto& registry = m_storage.registry();
    auto& parent = registry.get<ParentComponent>(entity.m_handle);
    const core::UUID previous_parent_id = parent.parent_id;
    parent.parent_id = {};
    registry.get<WorldTransformComponent>(entity.m_handle).dirty = true;
    updateWorldTransform(entity.m_handle);
    if (previous_parent_id.isValid()) {
        getLogChannel().debug("Detached entity {} from parent {}",
                entity.getUUID().toString(), previous_parent_id.toString());
    }
}

Entity SceneHierarchy::getParent(Entity entity) noexcept {
    if (!m_storage.isValid(entity)) {
        return {};
    }
    const auto& parent = m_storage.registry().get<ParentComponent>(entity.m_handle);
    if (!parent.parent_id.isValid()) {
        return {};
    }
    return m_storage.findEntity(parent.parent_id);
}

std::vector<Entity> SceneHierarchy::getChildren(Entity entity) {
    std::vector<Entity> children;
    if (!m_storage.isValid(entity)) {
        return children;
    }

    auto& registry = m_storage.registry();
    const core::UUID id = entity.getUUID();
    for (auto [handle, parent]: registry.view<ParentComponent>().each()) {
        if (parent.parent_id == id) {
            children.push_back(Entity{ handle, registry });
        }
    }
    return children;
}

const glm::mat4& SceneHierarchy::getWorldTransform(Entity entity) const {
    if (!m_storage.isValid(entity)) {
        throw std::invalid_argument(
                "The Entity does not belong to this Scene or is no longer valid.");
    }
    const auto& registry = std::as_const(m_storage).registry();
    return registry.get<WorldTransformComponent>(entity.m_handle).world;
}

void SceneHierarchy::updateWorldTransforms() {
    auto& registry = m_storage.registry();
    TransformUpdateStates states;
    states.reserve(registry.storage<entt::entity>().size());

    for (auto [entity]: registry.storage<entt::entity>().each()) {
        updateWorldTransform(entity, states);
    }
}

void SceneHierarchy::updateWorldTransform(entt::entity entity) {
    TransformUpdateStates states;
    updateWorldTransform(entity, states);
}

bool SceneHierarchy::updateWorldTransform(entt::entity entity, TransformUpdateStates& states) {
    const auto existing = states.find(entity);
    if (existing != states.end() && existing->second.visited) {
        return existing->second.changed;
    }
    if (existing != states.end() && existing->second.visiting) {
        getLogChannel().error("Detected a parent cycle while updating world transforms.");
        throw std::logic_error("Scene hierarchy contains a parent cycle.");
    }
    states[entity].visiting = true;

    auto& registry = m_storage.registry();
    const glm::mat4 local = registry.get<TransformComponent>(entity).getTransform();
    auto& world = registry.get<WorldTransformComponent>(entity);
    const auto& parent = registry.get<ParentComponent>(entity);

    entt::entity parent_handle = entt::null;
    core::UUID applied_parent_id;
    bool parent_changed = false;
    if (parent.parent_id.isValid()) {
        parent_handle = m_storage.resolveEntity(parent.parent_id);
        if (parent_handle != entt::null) {
            applied_parent_id = parent.parent_id;
            parent_changed = updateWorldTransform(parent_handle, states);
        }
    }

    const bool changed = world.dirty || !matricesEqual(world.local, local) ||
                         world.parent_id != applied_parent_id || parent_changed;
    if (changed) {
        world.world = parent_handle == entt::null
                              ? local
                              : registry.get<WorldTransformComponent>(parent_handle).world * local;
        world.local = local;
        world.parent_id = applied_parent_id;
        world.dirty = false;
    }

    TransformUpdateState& state = states.at(entity);
    state.visiting = false;
    state.visited = true;
    state.changed = changed;
    return changed;
}

void SceneHierarchy::collectSubtree(
        entt::entity root, std::vector<entt::entity>& subtree) const {
    const auto& registry = std::as_const(m_storage).registry();
    std::unordered_map<core::UUID, std::vector<entt::entity>> children_by_parent;
    children_by_parent.reserve(registry.storage<entt::entity>()->size());
    for (auto [handle, parent]: registry.view<ParentComponent>().each()) {
        if (parent.parent_id.isValid()) {
            children_by_parent[parent.parent_id].push_back(handle);
        }
    }

    const auto collect = [&](const auto& self, entt::entity entity) -> void {
        subtree.push_back(entity);
        const core::UUID id = registry.get<IDComponent>(entity).id;
        const auto found = children_by_parent.find(id);
        if (found == children_by_parent.end()) {
            return;
        }
        for (const entt::entity child: found->second) {
            self(self, child);
        }
    };
    collect(collect, root);
}

} // namespace arti::scene
