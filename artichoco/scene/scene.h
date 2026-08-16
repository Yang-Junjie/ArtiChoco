#pragma once
#include "entity.h"
#include "scene_cloner.h"
#include "scene_entity_storage.h"
#include "scene_hierarchy.h"
#include "scene_system_manager.h"

#include <entt/entt.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arti::scene {

class SceneSerializer;

class Scene {
public:
    Scene();
    ~Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = delete;
    Scene& operator=(Scene&&) = delete;

    Entity createEntity(std::string tag = "Entity");
    Entity createEntityWithUUID(core::UUID id, std::string tag = "Entity");

    void destroyEntity(Entity entity);

    Entity findEntity(core::UUID id) noexcept;
    Entity findEntityByTag(std::string_view tag) noexcept;

    bool containsEntity(core::UUID id) const noexcept;
    bool isValid(Entity entity) const noexcept;

    void setParent(Entity child, Entity parent);
    void detachFromParent(Entity entity);
    Entity getParent(Entity entity) noexcept;
    std::vector<Entity> getChildren(Entity entity);
    
    const glm::mat4& getWorldTransform(Entity entity) const;
    void updateWorldTransforms();

    void clearEntities();
    void copyEntitiesFrom(const Scene& source);

    template<typename Component>
    static void registerComponentCopy() {
        SceneCloner::registerComponent<Component>();
    }

    template<typename Type, typename... Other, typename... Exclude>
    [[nodiscard]] auto view(entt::exclude_t<Exclude...> exclude = entt::exclude_t<Exclude...>{}) {
        return m_entity_storage.view<Type, Other...>(exclude);
    }

    template<typename Type, typename... Other, typename... Exclude>
    [[nodiscard]] auto view(
            entt::exclude_t<Exclude...> exclude = entt::exclude_t<Exclude...>{}) const {
        return m_entity_storage.view<Type, Other...>(exclude);
    }

    template<typename System, typename... Args>
    auto& addSystem(SystemStage stage, Args&&... args) {
        return m_system_manager.addSystem<System>(stage, std::forward<Args>(args)...);
    }

    template<typename System>
    bool hasSystem() const noexcept {
        return m_system_manager.hasSystem<System>();
    }

    template<typename System>
    auto& getSystem() {
        return m_system_manager.getSystem<System>();
    }

    template<typename System>
    const auto& getSystem() const {
        return m_system_manager.getSystem<System>();
    }

    template<typename System>
    bool removeSystem() {
        return m_system_manager.removeSystem<System>();
    }

    template<typename System>
    void setSystemEnabled(bool enabled) {
        m_system_manager.setSystemEnabled<System>(enabled);
    }

    template<typename System>
    bool isSystemEnabled() const noexcept {
        return m_system_manager.isSystemEnabled<System>();
    }

    void runSystems(SystemStage stage, const UpdateContext& context);

private:
    friend class SceneCloner;
    friend class SceneSerializer;

    SceneEntityStorage m_entity_storage;
    SceneHierarchy m_hierarchy;
    SceneSystemManager m_system_manager;
};

} // namespace arti::scene
