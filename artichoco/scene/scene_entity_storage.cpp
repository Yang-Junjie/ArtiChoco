#include "scene_entity_storage.h"

#include "scene_log.h"

#include <stdexcept>
#include <utility>
#include <vector>

namespace arti::scene {

Entity SceneEntityStorage::createEntity(std::string tag) {
    core::UUID id;
    do {
        id = core::UUID::generate();
    } while (m_entity_lookup.contains(id));
    return createEntityWithUUID(id, std::move(tag));
}

Entity SceneEntityStorage::createEntityWithUUID(core::UUID id, std::string tag) {
    if (!id.isValid()) {
        getLogChannel().warn("Rejected creation of entity '{}': the UUID is invalid.", tag);
        throw std::invalid_argument("An Entity requires a valid UUID.");
    }
    if (m_entity_lookup.contains(id)) {
        getLogChannel().warn("Rejected creation of entity '{}' with duplicate UUID {}.",
                tag, id.toString());
        throw std::invalid_argument("The Entity UUID already exists in this Scene.");
    }

    const std::string tag_name = tag;
    const entt::entity handle = m_registry.create();
    try {
        m_registry.emplace<IDComponent>(handle, id);
        m_registry.emplace<TagComponent>(handle, std::move(tag));
        m_registry.emplace<TransformComponent>(handle);
        m_registry.emplace<ParentComponent>(handle);
        m_registry.emplace<WorldTransformComponent>(handle);
        indexEntity(handle);
    } catch (...) {
        m_registry.destroy(handle);
        throw;
    }
    getLogChannel().debug("Created entity '{}' ({})", tag_name, id.toString());
    return Entity{ handle, m_registry };
}

Entity SceneEntityStorage::findEntity(core::UUID id) noexcept {
    const entt::entity handle = resolveEntity(id);
    return handle == entt::null ? Entity{} : Entity{ handle, m_registry };
}

Entity SceneEntityStorage::findEntityByTag(std::string_view tag) noexcept {
    for (auto [handle, tag_component]: m_registry.view<TagComponent>().each()) {
        if (tag_component.tag == tag) {
            return Entity{ handle, m_registry };
        }
    }
    return {};
}

bool SceneEntityStorage::containsEntity(core::UUID id) const noexcept {
    return resolveEntity(id) != entt::null;
}

bool SceneEntityStorage::isValid(Entity entity) const noexcept {
    return entity.m_registry == &m_registry && entity.m_handle != entt::null &&
           m_registry.valid(entity.m_handle);
}

void SceneEntityStorage::clearEntities() {
    std::vector<entt::entity> entities;
    const auto& entity_storage = m_registry.storage<entt::entity>();
    entities.reserve(entity_storage.size());
    for (auto [entity]: entity_storage.each()) {
        entities.push_back(entity);
    }
    for (const entt::entity entity: entities) {
        destroyEntity(entity);
    }
    if (!entities.empty()) {
        getLogChannel().debug("Cleared {} entities", entities.size());
    }
}

entt::entity SceneEntityStorage::resolveEntity(core::UUID id) const noexcept {
    const auto found = m_entity_lookup.find(id);
    if (found == m_entity_lookup.end() || !m_registry.valid(found->second)) {
        return entt::null;
    }
    return found->second;
}

void SceneEntityStorage::indexEntity(entt::entity entity) {
    const core::UUID id = m_registry.get<IDComponent>(entity).id;
    const auto [_, inserted] = m_entity_lookup.emplace(id, entity);
    if (!inserted) {
        getLogChannel().error("Failed to index duplicate Entity UUID {}.", id.toString());
        throw std::logic_error("Failed to index the Entity UUID.");
    }
}

void SceneEntityStorage::destroyEntity(entt::entity entity) {
    const core::UUID id = m_registry.get<IDComponent>(entity).id;
    m_entity_lookup.erase(id);
    m_registry.destroy(entity);
}

void SceneEntityStorage::replaceWith(SceneEntityStorage&& source) {
    clearEntities();
    m_registry = std::move(source.m_registry);
    m_entity_lookup = std::move(source.m_entity_lookup);
}

} // namespace arti::scene
