#include "scene.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace arti::scene {

namespace {

bool isValidSystemStage(SystemStage stage) noexcept
{
    switch (stage) {
        case SystemStage::FixedUpdate:
        case SystemStage::Update:
        case SystemStage::LateUpdate:
        case SystemStage::RenderExtract:
            return true;
    }
    return false;
}

} // namespace

struct Scene::SystemStorage {
    struct Entry {
        SystemStage stage;
        std::type_index type;
        std::unique_ptr<SceneSystem> system;
        bool enabled{true};
    };

    std::vector<Entry> entries;
    bool executing{false};
    bool inLifecycleCallback{false};
};

Scene::Scene()
    : m_system_storage(std::make_unique<SystemStorage>())
{}

Scene::~Scene()
{
    m_system_storage->inLifecycleCallback = true;
    for (auto entry = m_system_storage->entries.rbegin();
         entry != m_system_storage->entries.rend();
         ++entry) {
        try {
            entry->system->onDetach(*this);
        } catch (...) {
            // Destruction must continue so every attached System is released.
        }
    }
}

Entity Scene::createEntity(std::string tag)
{
    core::UUID id;
    do {
        id = core::UUID::generate();
    } while (m_entity_lookup.contains(id));
    return createEntityWithUUID(id, std::move(tag));
}

Entity Scene::createEntityWithUUID(core::UUID id, std::string tag)
{
    if (!id.isValid()) {
        throw std::invalid_argument("An Entity requires a valid UUID.");
    }
    if (m_entity_lookup.contains(id)) {
        throw std::invalid_argument("The Entity UUID already exists in this Scene.");
    }

    const entt::entity handle = m_registry.create();
    try {
        m_registry.emplace<IDComponent>(handle, id);
        m_registry.emplace<TagComponent>(handle, std::move(tag));
        m_registry.emplace<TransformComponent>(handle);
        m_registry.emplace<ParentComponent>(handle);
        m_registry.emplace<WorldTransformComponent>(handle);
        const auto [_, inserted] = m_entity_lookup.emplace(id, handle);
        if (!inserted) {
            throw std::logic_error("Failed to index the Entity UUID.");
        }
    } catch (...) {
        m_registry.destroy(handle);
        throw;
    }
    return Entity{handle, m_registry};
}

void Scene::destroyEntity(Entity entity)
{
    if (!isValid(entity)) {
        throw std::invalid_argument("The Entity does not belong to this Scene or is no longer valid.");
    }

    std::vector<entt::entity> subtree;
    collectSubtree(entity.m_handle, subtree);
    for (auto it = subtree.rbegin(); it != subtree.rend(); ++it) {
        const core::UUID id = m_registry.get<IDComponent>(*it).id;
        m_entity_lookup.erase(id);
        m_registry.destroy(*it);
    }
}

void Scene::setParent(Entity child, Entity parent)
{
    if (!isValid(child) || !isValid(parent)) {
        throw std::invalid_argument("The Entity does not belong to this Scene or is no longer valid.");
    }
    if (child.m_handle == parent.m_handle) {
        throw std::invalid_argument("An Entity cannot be its own parent.");
    }

    for (entt::entity cursor = parent.m_handle; cursor != entt::null;) {
        if (cursor == child.m_handle) {
            throw std::invalid_argument("The requested parent relationship would create a cycle.");
        }
        const auto& parent_component = m_registry.get<ParentComponent>(cursor);
        if (!parent_component.parent_id.isValid()) {
            break;
        }
        cursor = resolveEntity(parent_component.parent_id);
    }

    m_registry.get<ParentComponent>(child.m_handle).parent_id = parent.getUUID();
    updateWorldTransform(child.m_handle);
}

void Scene::detachFromParent(Entity entity)
{
    if (!isValid(entity)) {
        throw std::invalid_argument("The Entity does not belong to this Scene or is no longer valid.");
    }
    m_registry.get<ParentComponent>(entity.m_handle).parent_id = {};
    updateWorldTransform(entity.m_handle);
}

Entity Scene::getParent(Entity entity) noexcept
{
    if (!isValid(entity)) {
        return {};
    }
    const auto& parent = m_registry.get<ParentComponent>(entity.m_handle);
    if (!parent.parent_id.isValid()) {
        return {};
    }
    const entt::entity parent_handle = resolveEntity(parent.parent_id);
    if (parent_handle == entt::null) {
        return {};
    }
    return Entity{parent_handle, m_registry};
}

std::vector<Entity> Scene::getChildren(Entity entity)
{
    std::vector<Entity> children;
    if (!isValid(entity)) {
        return children;
    }

    const core::UUID id = entity.getUUID();
    for (auto [handle, parent] : m_registry.view<ParentComponent>().each()) {
        if (parent.parent_id == id) {
            children.push_back(Entity{handle, m_registry});
        }
    }
    return children;
}

const glm::mat4& Scene::getWorldTransform(Entity entity) const
{
    if (!isValid(entity)) {
        throw std::invalid_argument("The Entity does not belong to this Scene or is no longer valid.");
    }
    return std::as_const(m_registry).get<WorldTransformComponent>(entity.m_handle).world;
}

void Scene::updateWorldTransforms()
{
    for (auto [entity, transform, world, parent] :
         m_registry.view<TransformComponent, WorldTransformComponent, ParentComponent>().each()) {
        updateWorldTransform(entity);
    }
}

entt::entity Scene::resolveEntity(const core::UUID& id) const noexcept
{
    const auto found = m_entity_lookup.find(id);
    if (found == m_entity_lookup.end() || !m_registry.valid(found->second)) {
        return entt::null;
    }
    return found->second;
}

void Scene::updateWorldTransform(entt::entity entity)
{
    auto& transform = m_registry.get<TransformComponent>(entity);
    auto& world = m_registry.get<WorldTransformComponent>(entity);

    const auto& parent = m_registry.get<ParentComponent>(entity);
    if (!parent.parent_id.isValid()) {
        world.world = transform.getTransform();
        world.dirty = false;
        return;
    }

    const entt::entity parent_handle = resolveEntity(parent.parent_id);
    if (parent_handle == entt::null) {
        world.world = transform.getTransform();
        world.dirty = false;
        return;
    }

    updateWorldTransform(parent_handle);
    world.world = m_registry.get<WorldTransformComponent>(parent_handle).world * transform.getTransform();
    world.dirty = false;
}

void Scene::collectSubtree(entt::entity root, std::vector<entt::entity>& subtree) const
{
    subtree.push_back(root);
    const core::UUID root_id = m_registry.get<IDComponent>(root).id;
    for (auto [handle, parent] : m_registry.view<ParentComponent>().each()) {
        if (handle != root && parent.parent_id == root_id) {
            collectSubtree(handle, subtree);
        }
    }
}

Entity Scene::findEntity(core::UUID id) noexcept
{
    const auto found = m_entity_lookup.find(id);
    if (found == m_entity_lookup.end() || !m_registry.valid(found->second)) {
        return {};
    }
    return Entity{found->second, m_registry};
}

Entity Scene::findEntityByTag(std::string_view tag) noexcept
{
    for (auto [handle, tag_component] : m_registry.view<TagComponent>().each()) {
        if (tag_component.tag == tag) {
            return Entity{handle, m_registry};
        }
    }
    return {};
}

bool Scene::containsEntity(core::UUID id) const noexcept
{
    const auto found = m_entity_lookup.find(id);
    return found != m_entity_lookup.end() && m_registry.valid(found->second);
}

bool Scene::isValid(Entity entity) const noexcept
{
    return entity.m_registry == &m_registry && entity.m_handle != entt::null && m_registry.valid(entity.m_handle);
}

std::unordered_map<entt::id_type, Scene::ComponentCopyRegistration>& Scene::copyRegistry()
{
    static std::unordered_map<entt::id_type, ComponentCopyRegistration> registry = [] {
        std::unordered_map<entt::id_type, ComponentCopyRegistration> initial;
        registerCopyInto<IDComponent>(initial);
        registerCopyInto<TagComponent>(initial);
        registerCopyInto<TransformComponent>(initial);
        registerCopyInto<ParentComponent>(initial);
        registerCopyInto<WorldTransformComponent>(initial);
        return initial;
    }();
    return registry;
}

void Scene::clearEntities()
{
    std::vector<entt::entity> entities;
    const auto& entity_storage = m_registry.storage<entt::entity>();
    entities.reserve(entity_storage.size());
    for (auto [entity] : entity_storage.each()) {
        entities.push_back(entity);
    }
    for (const entt::entity entity : entities) {
        const core::UUID id = m_registry.get<IDComponent>(entity).id;
        m_entity_lookup.erase(id);
        m_registry.destroy(entity);
    }
}

void Scene::copyEntitiesFrom(const Scene& source)
{
    if (this == &source) {
        throw std::invalid_argument("A Scene cannot copy entities from itself.");
    }

    clearEntities();

    std::unordered_map<entt::entity, entt::entity> entity_map;
    const auto* entity_storage = source.m_registry.storage<entt::entity>();
    entity_map.reserve(entity_storage->size());
    for (auto [source_entity] : entity_storage->each()) {
        const entt::entity destination_entity = m_registry.create();
        entity_map.emplace(source_entity, destination_entity);
    }

    for (const auto& [id, registration] : copyRegistry()) {
        const auto* storage = source.m_registry.storage(id);
        if (storage == nullptr) {
            continue;
        }
        for (const auto& [source_entity, destination_entity] : entity_map) {
            if (storage->contains(source_entity)) {
                registration.copy_fn(source.m_registry, m_registry, source_entity, destination_entity);
            }
        }
    }

    for (const auto& [source_entity, destination_entity] : entity_map) {
        m_entity_lookup.emplace(m_registry.get<IDComponent>(destination_entity).id, destination_entity);
    }
}

SceneSystem& Scene::registerSystem(
    SystemStage stage,
    std::type_index type,
    std::unique_ptr<SceneSystem> system)
{
    if (!isValidSystemStage(stage)) {
        throw std::invalid_argument("The requested SystemStage is not valid.");
    }
    if (m_system_storage->executing || m_system_storage->inLifecycleCallback) {
        throw std::logic_error("Systems cannot be added during System execution or lifecycle callbacks.");
    }
    if (findSystem(type) != nullptr) {
        throw std::logic_error("A System of this type is already registered with the Scene.");
    }

    m_system_storage->entries.push_back(SystemStorage::Entry{stage, type, std::move(system)});
    SceneSystem& registered = *m_system_storage->entries.back().system;

    m_system_storage->inLifecycleCallback = true;
    try {
        registered.onAttach(*this);
    } catch (...) {
        m_system_storage->inLifecycleCallback = false;
        m_system_storage->entries.pop_back();
        throw;
    }
    m_system_storage->inLifecycleCallback = false;
    return registered;
}

SceneSystem* Scene::findSystem(std::type_index type) noexcept
{
    const auto found = std::find_if(
        m_system_storage->entries.begin(),
        m_system_storage->entries.end(),
        [type](const SystemStorage::Entry& entry) { return entry.type == type; });
    return found == m_system_storage->entries.end() ? nullptr : found->system.get();
}

const SceneSystem* Scene::findSystem(std::type_index type) const noexcept
{
    const auto found = std::find_if(
        m_system_storage->entries.cbegin(),
        m_system_storage->entries.cend(),
        [type](const SystemStorage::Entry& entry) { return entry.type == type; });
    return found == m_system_storage->entries.cend() ? nullptr : found->system.get();
}

bool Scene::removeSystem(std::type_index type)
{
    if (m_system_storage->executing || m_system_storage->inLifecycleCallback) {
        throw std::logic_error("Systems cannot be removed during System execution or lifecycle callbacks.");
    }

    const auto found = std::find_if(
        m_system_storage->entries.begin(),
        m_system_storage->entries.end(),
        [type](const SystemStorage::Entry& entry) { return entry.type == type; });
    if (found == m_system_storage->entries.end()) {
        return false;
    }

    m_system_storage->inLifecycleCallback = true;
    try {
        found->system->onDetach(*this);
    } catch (...) {
        m_system_storage->inLifecycleCallback = false;
        throw;
    }
    m_system_storage->inLifecycleCallback = false;
    m_system_storage->entries.erase(found);
    return true;
}

void Scene::setSystemEnabled(std::type_index type, bool enabled)
{
    const auto found = std::find_if(
        m_system_storage->entries.begin(),
        m_system_storage->entries.end(),
        [type](const SystemStorage::Entry& entry) { return entry.type == type; });
    if (found == m_system_storage->entries.end()) {
        throw std::out_of_range("The requested System is not registered with this Scene.");
    }
    found->enabled = enabled;
}

bool Scene::isSystemEnabled(std::type_index type) const noexcept
{
    const auto found = std::find_if(
        m_system_storage->entries.cbegin(),
        m_system_storage->entries.cend(),
        [type](const SystemStorage::Entry& entry) { return entry.type == type; });
    return found != m_system_storage->entries.cend() && found->enabled;
}

void Scene::runSystems(SystemStage stage, const UpdateContext& context)
{
    if (!isValidSystemStage(stage)) {
        throw std::invalid_argument("The requested SystemStage is not valid.");
    }
    if (m_system_storage->executing || m_system_storage->inLifecycleCallback) {
        throw std::logic_error("Scene System execution cannot be nested.");
    }

    m_system_storage->executing = true;
    try {
        updateWorldTransforms();
        for (const SystemStorage::Entry& entry : m_system_storage->entries) {
            if (entry.stage == stage && entry.enabled) {
                entry.system->onUpdate(*this, context);
            }
        }
    } catch (...) {
        m_system_storage->executing = false;
        throw;
    }
    m_system_storage->executing = false;
}

} // namespace arti::scene
