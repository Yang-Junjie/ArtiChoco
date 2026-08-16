#pragma once

#include <entt/entt.hpp>
#include <type_traits>
#include <unordered_map>

namespace arti::scene {

class Scene;

class SceneCloner {
public:
    template<typename Component>
    static void registerComponent() {
        static_assert(std::is_copy_constructible_v<Component>,
                "Scene components must be copy-constructible to support scene cloning.");
        registerCopyInto<Component>(copyRegistry());
    }

    static void clone(const Scene& source, Scene& destination);

private:
    using ComponentCopyFn = void (*)(const entt::registry&, entt::registry&, entt::entity,entt::entity);

    template<typename Component>
    static void registerCopyInto(std::unordered_map<entt::id_type, ComponentCopyFn>& registry) {
        registry.insert_or_assign(entt::type_hash<Component>::value(),
                [](const entt::registry& source, entt::registry& destination,
                   entt::entity source_entity, entt::entity destination_entity) 
                {
                    destination.emplace<Component>(destination_entity,source.get<Component>(source_entity));
                });
    }

    static std::unordered_map<entt::id_type, ComponentCopyFn>& copyRegistry();
};

} // namespace arti::scene
