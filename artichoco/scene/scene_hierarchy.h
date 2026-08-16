#pragma once
#include "scene_entity_storage.h"

#include <cstdint>
#include <entt/entt.hpp>
#include <unordered_map>
#include <vector>

namespace arti::scene {

class Scene;

class SceneHierarchy {
public:
    explicit SceneHierarchy(SceneEntityStorage& storage) noexcept;

    SceneHierarchy(const SceneHierarchy&) = delete;
    SceneHierarchy& operator=(const SceneHierarchy&) = delete;
    SceneHierarchy(SceneHierarchy&&) = delete;
    SceneHierarchy& operator=(SceneHierarchy&&) = delete;

    void setParent(Entity child, Entity parent);
    void detachFromParent(Entity entity);
    Entity getParent(Entity entity) noexcept;
    std::vector<Entity> getChildren(Entity entity);

    const glm::mat4& getWorldTransform(Entity entity) const;
    void updateWorldTransforms();

private:
    friend class Scene;

    struct TransformUpdateState {
        bool visiting{ false };
        bool visited{ false };
        bool changed{ false };
    };

    using TransformUpdateStates =
            std::unordered_map<entt::entity, TransformUpdateState>;

    void updateWorldTransform(entt::entity entity);
    bool updateWorldTransform(entt::entity entity, TransformUpdateStates& states);
    void collectSubtree(entt::entity root, std::vector<entt::entity>& subtree) const;

    SceneEntityStorage& m_storage;
};

} // namespace arti::scene
