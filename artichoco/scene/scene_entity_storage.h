#pragma once
#include "entity.h"

#include <entt/entt.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

namespace arti::scene {

class Scene;
class SceneCloner;
class SceneHierarchy;
class SceneSerializer;

class SceneEntityStorage {
public:
    SceneEntityStorage() = default;

    SceneEntityStorage(const SceneEntityStorage&) = delete;
    SceneEntityStorage& operator=(const SceneEntityStorage&) = delete;
    SceneEntityStorage(SceneEntityStorage&&) = delete;
    SceneEntityStorage& operator=(SceneEntityStorage&&) = delete;

    Entity createEntity(std::string tag = "Entity");
    Entity createEntityWithUUID(core::UUID id, std::string tag = "Entity");
    Entity findEntity(core::UUID id) noexcept;
    Entity findEntityByTag(std::string_view tag) noexcept;

    bool containsEntity(core::UUID id) const noexcept;
    bool isValid(Entity entity) const noexcept;
    void clearEntities();

    template<typename Type, typename... Other, typename... Exclude>
    [[nodiscard]] auto view(entt::exclude_t<Exclude...> exclude = entt::exclude_t<Exclude...>{}) {
        return m_registry.view<detail::SceneViewComponent<Type>,
                detail::SceneViewComponent<Other>...>(exclude);
    }

    template<typename Type, typename... Other, typename... Exclude>
    [[nodiscard]] auto view(
            entt::exclude_t<Exclude...> exclude = entt::exclude_t<Exclude...>{}) const {
        return m_registry.view<Type, Other...>(exclude);
    }

private:
    friend class Scene;
    friend class SceneCloner;
    friend class SceneHierarchy;
    friend class SceneSerializer;

    entt::registry& registry() noexcept { return m_registry; }
    const entt::registry& registry() const noexcept { return m_registry; }

    entt::entity resolveEntity(core::UUID id) const noexcept;
    void indexEntity(entt::entity entity);
    void destroyEntity(entt::entity entity);
    void replaceWith(SceneEntityStorage&& source);

    entt::registry m_registry;
    std::unordered_map<core::UUID, entt::entity> m_entity_lookup;
};

} // namespace arti::scene
