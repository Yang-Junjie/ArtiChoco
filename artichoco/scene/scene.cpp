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
    m_entity_lookup.erase(entity.getUUID());
    m_registry.destroy(entity.m_handle);
}

Entity Scene::findEntity(core::UUID id) noexcept
{
    const auto found = m_entity_lookup.find(id);
    if (found == m_entity_lookup.end() || !m_registry.valid(found->second)) {
        return {};
    }
    return Entity{found->second, m_registry};
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
        for (const SystemStorage::Entry& entry : m_system_storage->entries) {
            if (entry.stage == stage) {
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
