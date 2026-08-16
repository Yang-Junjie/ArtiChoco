#include "scene_cloner.h"

#include "components.h"
#include "scene.h"
#include "scene_log.h"

#include <stdexcept>
#include <unordered_map>

namespace arti::scene {

std::unordered_map<entt::id_type, SceneCloner::ComponentCopyFn>& SceneCloner::copyRegistry() {
    static std::unordered_map<entt::id_type, ComponentCopyFn> registry = [] {
        std::unordered_map<entt::id_type, ComponentCopyFn> initial;
        registerCopyInto<IDComponent>(initial);
        registerCopyInto<TagComponent>(initial);
        registerCopyInto<TransformComponent>(initial);
        registerCopyInto<ParentComponent>(initial);
        registerCopyInto<WorldTransformComponent>(initial);
        return initial;
    }();
    return registry;
}

void SceneCloner::clone(const Scene& source, Scene& destination) {
    if (&source == &destination) {
        getLogChannel().warn("Rejected an attempt to copy a Scene into itself.");
        throw std::invalid_argument("A Scene cannot copy entities from itself.");
    }

    auto& destination_storage = destination.m_entity_storage;
    const auto& source_registry = source.m_entity_storage.registry();
    auto& destination_registry = destination_storage.registry();
    destination_storage.clearEntities();

    std::unordered_map<entt::entity, entt::entity> entity_map;
    const auto* entity_storage = source_registry.storage<entt::entity>();
    entity_map.reserve(entity_storage->size());
    for (auto [source_entity]: entity_storage->each()) {
        const entt::entity destination_entity = destination_registry.create();
        entity_map.emplace(source_entity, destination_entity);
    }

    size_t skipped_component_types = 0;
    for (const auto& [id, storage]: source_registry.storage()) {
        if (id == entt::type_hash<entt::entity>::value() || copyRegistry().contains(id)) {
            continue;
        }
        if (storage.size() > 0) {
            ++skipped_component_types;
            getLogChannel().warn("Skipped copying a component type with storage id {} because it "
                                 "is not registered for copy.",
                    id);
        }
    }

    for (const auto& [id, copy_component]: copyRegistry()) {
        const auto* storage = source_registry.storage(id);
        if (storage == nullptr) {
            continue;
        }
        for (const auto& [source_entity, destination_entity]: entity_map) {
            if (storage->contains(source_entity)) {
                copy_component(source_registry, destination_registry, source_entity,
                        destination_entity);
            }
        }
    }

    for (const auto& entity_pair: entity_map) {
        destination_storage.indexEntity(entity_pair.second);
    }
    getLogChannel().debug("Copied {} entities from another Scene ({} component types skipped)",
            entity_map.size(), skipped_component_types);
}

} // namespace arti::scene
