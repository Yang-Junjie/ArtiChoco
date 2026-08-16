#include "scene.h"
#include "scene_log.h"

#include <stdexcept>
#include <utility>
#include <vector>

namespace arti::scene {

Scene::Scene()
        : m_hierarchy(m_entity_storage),
          m_system_manager(*this) {}

Scene::~Scene() = default;

Entity Scene::createEntity(std::string tag) {
    return m_entity_storage.createEntity(std::move(tag));
}

Entity Scene::createEntityWithUUID(core::UUID id, std::string tag) {
    return m_entity_storage.createEntityWithUUID(id, std::move(tag));
}

void Scene::destroyEntity(Entity entity) {
    if (!isValid(entity)) {
        getLogChannel().warn(
                "Rejected entity destruction: the Entity is invalid or belongs to another Scene.");
        throw std::invalid_argument(
                "The Entity does not belong to this Scene or is no longer valid.");
    }

    auto& registry = m_entity_storage.registry();
    std::vector<entt::entity> subtree;
    m_hierarchy.collectSubtree(entity.m_handle, subtree);
    const std::string tag = registry.get<TagComponent>(entity.m_handle).tag;
    for (auto it = subtree.rbegin(); it != subtree.rend(); ++it) {
        m_entity_storage.destroyEntity(*it);
    }
    getLogChannel().debug("Destroyed entity '{}' ({} including descendants)", tag, subtree.size());
}

Entity Scene::findEntity(core::UUID id) noexcept {
    return m_entity_storage.findEntity(id);
}

Entity Scene::findEntityByTag(std::string_view tag) noexcept {
    return m_entity_storage.findEntityByTag(tag);
}

bool Scene::containsEntity(core::UUID id) const noexcept {
    return m_entity_storage.containsEntity(id);
}

bool Scene::isValid(Entity entity) const noexcept {
    return m_entity_storage.isValid(entity);
}

void Scene::setParent(Entity child, Entity parent) {
    m_hierarchy.setParent(child, parent);
}

void Scene::detachFromParent(Entity entity) {
    m_hierarchy.detachFromParent(entity);
}

Entity Scene::getParent(Entity entity) noexcept {
    return m_hierarchy.getParent(entity);
}

std::vector<Entity> Scene::getChildren(Entity entity) {
    return m_hierarchy.getChildren(entity);
}

const glm::mat4& Scene::getWorldTransform(Entity entity) const {
    return m_hierarchy.getWorldTransform(entity);
}

void Scene::updateWorldTransforms() {
    m_hierarchy.updateWorldTransforms();
}

void Scene::clearEntities() {
    m_entity_storage.clearEntities();
}

void Scene::copyEntitiesFrom(const Scene& source) {
    SceneCloner::clone(source, *this);
}

void Scene::runSystems(SystemStage stage, const UpdateContext& context) {
    m_system_manager.runSystems(stage, context);
}

} // namespace arti::scene
