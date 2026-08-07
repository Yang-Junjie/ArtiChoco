#pragma once
#include "entity.h"
#include "system.h"

#include <entt/entt.hpp>

#include <concepts>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace arti::scene {

namespace detail {

template <typename Component>
inline constexpr bool isMutableIDComponent =
    std::is_same_v<std::remove_cvref_t<Component>, IDComponent> &&
    !std::is_const_v<std::remove_reference_t<Component>>;

} // namespace detail

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
    bool containsEntity(core::UUID id) const noexcept;
    bool isValid(Entity entity) const noexcept;

    template <typename Type, typename... Other, typename... Exclude>
    [[nodiscard]] auto view(
        entt::exclude_t<Exclude...> exclude = entt::exclude_t<Exclude...>{})
    {
        static_assert(!detail::isMutableIDComponent<Type> &&
                          (... && !detail::isMutableIDComponent<Other>),
                      "IDComponent is read-only because Scene owns the UUID lookup.");
        return m_registry.view<Type, Other...>(exclude);
    }

    template <typename Type, typename... Other, typename... Exclude>
    [[nodiscard]] auto view(
        entt::exclude_t<Exclude...> exclude = entt::exclude_t<Exclude...>{}) const
    {
        return m_registry.view<Type, Other...>(exclude);
    }

    template <typename System, typename... Args>
    std::remove_cvref_t<System>& addSystem(SystemStage stage, Args&&... args)
    {
        using SystemType = std::remove_cvref_t<System>;
        static_assert(std::derived_from<SystemType, SceneSystem>,
                      "A Scene System must derive from SceneSystem.");

        auto system = std::make_unique<SystemType>(std::forward<Args>(args)...);
        SceneSystem& registered = registerSystem(
            stage, std::type_index{typeid(SystemType)}, std::move(system));
        return static_cast<SystemType&>(registered);
    }

    template <typename System>
    bool hasSystem() const noexcept
    {
        using SystemType = std::remove_cvref_t<System>;
        static_assert(std::derived_from<SystemType, SceneSystem>,
                      "A Scene System must derive from SceneSystem.");
        return findSystem(std::type_index{typeid(SystemType)}) != nullptr;
    }

    template <typename System>
    std::remove_cvref_t<System>& getSystem()
    {
        using SystemType = std::remove_cvref_t<System>;
        static_assert(std::derived_from<SystemType, SceneSystem>,
                      "A Scene System must derive from SceneSystem.");

        SceneSystem* system = findSystem(std::type_index{typeid(SystemType)});
        if (system == nullptr) {
            throw std::out_of_range("The requested System is not registered with this Scene.");
        }
        return static_cast<SystemType&>(*system);
    }

    template <typename System>
    const std::remove_cvref_t<System>& getSystem() const
    {
        using SystemType = std::remove_cvref_t<System>;
        static_assert(std::derived_from<SystemType, SceneSystem>,
                      "A Scene System must derive from SceneSystem.");

        const SceneSystem* system = findSystem(std::type_index{typeid(SystemType)});
        if (system == nullptr) {
            throw std::out_of_range("The requested System is not registered with this Scene.");
        }
        return static_cast<const SystemType&>(*system);
    }

    template <typename System>
    bool removeSystem()
    {
        using SystemType = std::remove_cvref_t<System>;
        static_assert(std::derived_from<SystemType, SceneSystem>,
                      "A Scene System must derive from SceneSystem.");
        return removeSystem(std::type_index{typeid(SystemType)});
    }

    void runSystems(SystemStage stage, const UpdateContext& context);

private:
    struct SystemStorage;

    SceneSystem& registerSystem(
        SystemStage stage,
        std::type_index type,
        std::unique_ptr<SceneSystem> system);
    SceneSystem* findSystem(std::type_index type) noexcept;
    const SceneSystem* findSystem(std::type_index type) const noexcept;
    bool removeSystem(std::type_index type);

    entt::registry m_registry;
    std::unordered_map<core::UUID, entt::entity> m_entity_lookup;
    std::unique_ptr<SystemStorage> m_system_storage;
};

} // namespace arti::scene
