#include "scene.h"
#include "scene_log.h"

#include <stdexcept>
#include <unordered_map>
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

Entity Scene::duplicateEntity(Entity entity) {
    if (!isValid(entity)) {
        getLogChannel().warn(
                "Rejected entity duplication: the Entity is invalid or belongs to another Scene.");
        throw std::invalid_argument(
                "The Entity does not belong to this Scene or is no longer valid.");
    }

    auto& registry = m_entity_storage.registry();
    std::vector<entt::entity> subtree;
    // 前序：根一定在最前面，所以下面 copies.front() 就是新的根。
    m_hierarchy.collectSubtree(entity.m_handle, subtree);

    // 旧 UUID → 新 UUID。父级引用存的就是 UUID（不是 handle），所以「把子树内部的父子关系
    // 接到副本上」就是查一次这张表，不用维护任何句柄映射。
    std::unordered_map<core::UUID, core::UUID> id_map;
    id_map.reserve(subtree.size());
    std::vector<entt::entity> copies;
    copies.reserve(subtree.size());

    size_t skipped_components = 0;
    for (const entt::entity source: subtree) {
        const entt::entity copy = registry.create();
        try {
            skipped_components += SceneCloner::copyComponents(registry, source, copy);

            // 身份归场景所有，不能跟着拷。indexEntity 必须**在**改完 IDComponent 之后 ——
            // 早一步登记的是源的 UUID，会和源自己撞在查找表的同一个键上。
            core::UUID id;
            do {
                id = core::UUID::generate();
            } while (m_entity_storage.containsEntity(id));
            registry.get<IDComponent>(copy).id = id;
            m_entity_storage.indexEntity(copy);

            // 世界变换是派生数据，拷过来的那份是按**源的**父级算的。标脏让
            // updateWorldTransforms() 重算，不指望它自己发现不一致。
            registry.get<WorldTransformComponent>(copy).dirty = true;

            id_map.emplace(registry.get<IDComponent>(source).id, id);
            copies.push_back(copy);
        } catch (...) {
            registry.destroy(copy);
            throw;
        }
    }

    // 查得到的是子树内部的父子关系；查不到的只有子树的根 —— 它的父级在子树外面，保持原样，
    // 于是副本自然成了源的兄弟。根不需要开特例。
    for (const entt::entity copy: copies) {
        auto& parent = registry.get<ParentComponent>(copy);
        const auto found = id_map.find(parent.parent_id);
        if (found != id_map.end()) {
            parent.parent_id = found->second;
        }
    }

    if (skipped_components > 0) {
        getLogChannel().warn("Duplicated an entity carrying {} component instances that are not "
                             "registered for copy; the copies are missing them.",
                skipped_components);
    }
    getLogChannel().debug("Duplicated entity '{}' ({} including descendants)",
            registry.get<TagComponent>(entity.m_handle).tag, copies.size());
    return Entity{ copies.front(), registry };
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
