#pragma once
#include "entity.h"
#include "system.h"

#include <concepts>
#include <entt/entt.hpp>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arti::scene {

class SceneSerializer;

namespace detail {

template<typename Component>
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
    Entity findEntityByTag(std::string_view tag) noexcept;

    bool containsEntity(core::UUID id) const noexcept;
    bool isValid(Entity entity) const noexcept;

    void setParent(Entity child, Entity parent);
    void detachFromParent(Entity entity);
    Entity getParent(Entity entity) noexcept;
    std::vector<Entity> getChildren(Entity entity);
    const glm::mat4& getWorldTransform(Entity entity) const;
    void updateWorldTransforms();

    template<typename Component>
    static void registerComponentCopy() {
        static_assert(std::is_copy_constructible_v<Component>,
                "Scene components must be copy-constructible to support scene copying.");
        registerCopyInto<Component>(copyRegistry());
    }

    void clearEntities();
    void copyEntitiesFrom(const Scene& source);

    template<typename Type, typename... Other, typename... Exclude>
    [[nodiscard]] auto view(entt::exclude_t<Exclude...> exclude = entt::exclude_t<Exclude...>{}) {
        static_assert(!detail::isMutableIDComponent<Type> &&
                              (... && !detail::isMutableIDComponent<Other>),
                "IDComponent is read-only because Scene owns the UUID lookup.");
        return m_registry.view<Type, Other...>(exclude);
    }

    template<typename Type, typename... Other, typename... Exclude>
    [[nodiscard]] auto view(
            entt::exclude_t<Exclude...> exclude = entt::exclude_t<Exclude...>{}) const {
        return m_registry.view<Type, Other...>(exclude);
    }

    template<typename System, typename... Args>
    std::remove_cvref_t<System>& addSystem(SystemStage stage, Args&&... args) {
        using SystemType = std::remove_cvref_t<System>;
        static_assert(std::derived_from<SystemType, SceneSystem>,
                "A Scene System must derive from SceneSystem.");

        auto system = std::make_unique<SystemType>(std::forward<Args>(args)...);
        SceneSystem& registered =
                registerSystem(stage, std::type_index{ typeid(SystemType) }, std::move(system));
        return static_cast<SystemType&>(registered);
    }

    template<typename System>
    bool hasSystem() const noexcept {
        using SystemType = std::remove_cvref_t<System>;
        static_assert(std::derived_from<SystemType, SceneSystem>,
                "A Scene System must derive from SceneSystem.");
        return findSystem(std::type_index{ typeid(SystemType) }) != nullptr;
    }

    template<typename System>
    std::remove_cvref_t<System>& getSystem() {
        using SystemType = std::remove_cvref_t<System>;
        static_assert(std::derived_from<SystemType, SceneSystem>,
                "A Scene System must derive from SceneSystem.");

        SceneSystem* system = findSystem(std::type_index{ typeid(SystemType) });
        if (system == nullptr) {
            throw std::out_of_range("The requested System is not registered with this Scene.");
        }
        return static_cast<SystemType&>(*system);
    }

    template<typename System>
    const std::remove_cvref_t<System>& getSystem() const {
        using SystemType = std::remove_cvref_t<System>;
        static_assert(std::derived_from<SystemType, SceneSystem>,
                "A Scene System must derive from SceneSystem.");

        const SceneSystem* system = findSystem(std::type_index{ typeid(SystemType) });
        if (system == nullptr) {
            throw std::out_of_range("The requested System is not registered with this Scene.");
        }
        return static_cast<const SystemType&>(*system);
    }

    template<typename System>
    bool removeSystem() {
        using SystemType = std::remove_cvref_t<System>;
        static_assert(std::derived_from<SystemType, SceneSystem>,
                "A Scene System must derive from SceneSystem.");
        return removeSystem(std::type_index{ typeid(SystemType) });
    }

    template<typename System>
    void setSystemEnabled(bool enabled) {
        using SystemType = std::remove_cvref_t<System>;
        static_assert(std::derived_from<SystemType, SceneSystem>,
                "A Scene System must derive from SceneSystem.");
        setSystemEnabled(std::type_index{ typeid(SystemType) }, enabled);
    }

    template<typename System>
    bool isSystemEnabled() const noexcept {
        using SystemType = std::remove_cvref_t<System>;
        static_assert(std::derived_from<SystemType, SceneSystem>,
                "A Scene System must derive from SceneSystem.");
        return isSystemEnabled(std::type_index{ typeid(SystemType) });
    }

    void runSystems(SystemStage stage, const UpdateContext& context);

private:
    friend class SceneSerializer;

    struct SystemStorage;

    using ComponentCopyFn =
            std::function<void(const entt::registry&, entt::registry&, entt::entity, entt::entity)>;

    struct ComponentCopyRegistration {
        entt::id_type id;
        ComponentCopyFn copy_fn;
    };

    template<typename Component>
    static void registerCopyInto(
            std::unordered_map<entt::id_type, ComponentCopyRegistration>& registry) {
        registry.insert_or_assign(entt::type_hash<Component>::value(),
                ComponentCopyRegistration{
                    entt::type_hash<Component>::value(),
                    [](const entt::registry& source, entt::registry& destination,
                            entt::entity source_entity, entt::entity destination_entity) {
                        destination.emplace<Component>(destination_entity,
                                source.get<Component>(source_entity));
                    },
                });
    }

    static std::unordered_map<entt::id_type, ComponentCopyRegistration>& copyRegistry();

    SceneSystem& registerSystem(SystemStage stage, std::type_index type,
            std::unique_ptr<SceneSystem> system);
    SceneSystem* findSystem(std::type_index type) noexcept;
    const SceneSystem* findSystem(std::type_index type) const noexcept;
    bool removeSystem(std::type_index type);
    void setSystemEnabled(std::type_index type, bool enabled);
    bool isSystemEnabled(std::type_index type) const noexcept;

    entt::entity resolveEntity(const core::UUID& id) const noexcept;
    void updateWorldTransform(entt::entity entity);
    void collectSubtree(entt::entity root, std::vector<entt::entity>& subtree) const;

    entt::registry m_registry;
    std::unordered_map<core::UUID, entt::entity> m_entity_lookup;
    std::unique_ptr<SystemStorage> m_system_storage;
};

} // namespace arti::scene
