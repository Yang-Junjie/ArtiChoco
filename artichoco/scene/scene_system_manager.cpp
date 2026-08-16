#include "scene_system_manager.h"

#include "scene.h"
#include "scene_log.h"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace arti::scene {
namespace {

bool isValidSystemStage(SystemStage stage) noexcept {
    switch (stage) {
        case SystemStage::FixedUpdate:
        case SystemStage::Update:
        case SystemStage::LateUpdate:
        case SystemStage::RenderExtract:
            return true;
    }
    return false;
}

std::string_view systemStageName(SystemStage stage) noexcept {
    switch (stage) {
        case SystemStage::FixedUpdate:
            return "FixedUpdate";
        case SystemStage::Update:
            return "Update";
        case SystemStage::LateUpdate:
            return "LateUpdate";
        case SystemStage::RenderExtract:
            return "RenderExtract";
    }
    return "Invalid";
}

void logCallbackFailure(std::type_index type, std::string_view callback,
        const std::exception& exception) {
    getLogChannel().error(
            "System '{}' failed in {}: {}", type.name(), callback, exception.what());
}

void logUnknownCallbackFailure(std::type_index type, std::string_view callback) {
    getLogChannel().error("System '{}' failed in {} with an unknown exception.",
            type.name(), callback);
}

} // namespace

struct SceneSystemManager::Storage {
    struct Entry {
        SystemStage stage;
        std::type_index type;
        std::unique_ptr<SceneSystem> system;
        bool enabled{ true };
    };

    std::vector<Entry> entries;
    bool executing{ false };
    bool inLifecycleCallback{ false };
};

SceneSystemManager::SceneSystemManager(Scene& scene)
        : m_scene(scene),
          m_storage(std::make_unique<Storage>()) {}

SceneSystemManager::~SceneSystemManager() {
    m_storage->inLifecycleCallback = true;
    for (auto entry = m_storage->entries.rbegin(); entry != m_storage->entries.rend(); ++entry) {
        try {
            entry->system->onDetach(m_scene);
        } catch (const std::exception& exception) {
            logCallbackFailure(entry->type, "onDetach during Scene shutdown", exception);
            // Destruction must continue so every attached System is released.
        } catch (...) {
            logUnknownCallbackFailure(entry->type, "onDetach during Scene shutdown");
            // Destruction must continue so every attached System is released.
        }
    }
}

SceneSystem& SceneSystemManager::registerSystem(SystemStage stage, std::type_index type,
        std::unique_ptr<SceneSystem> system) {
    if (!isValidSystemStage(stage)) {
        throw std::invalid_argument("The requested SystemStage is not valid.");
    }
    if (m_storage->executing || m_storage->inLifecycleCallback) {
        getLogChannel().warn(
                "Rejected registration of System '{}' during System execution or a lifecycle callback.",
                type.name());
        throw std::logic_error(
                "Systems cannot be added during System execution or lifecycle callbacks.");
    }
    if (findSystem(type) != nullptr) {
        getLogChannel().warn("Rejected duplicate System registration: {}", type.name());
        throw std::logic_error("A System of this type is already registered with the Scene.");
    }

    m_storage->entries.push_back(Storage::Entry{ stage, type, std::move(system) });
    SceneSystem& registered = *m_storage->entries.back().system;

    m_storage->inLifecycleCallback = true;
    try {
        registered.onAttach(m_scene);
    } catch (const std::exception& exception) {
        logCallbackFailure(type, "onAttach", exception);
        m_storage->inLifecycleCallback = false;
        m_storage->entries.pop_back();
        throw;
    } catch (...) {
        logUnknownCallbackFailure(type, "onAttach");
        m_storage->inLifecycleCallback = false;
        m_storage->entries.pop_back();
        throw;
    }
    m_storage->inLifecycleCallback = false;
    getLogChannel().debug(
            "Registered System '{}' at stage {}", type.name(), systemStageName(stage));
    return registered;
}

SceneSystem* SceneSystemManager::findSystem(std::type_index type) noexcept {
    const auto found = std::find_if(m_storage->entries.begin(), m_storage->entries.end(),
            [type](const Storage::Entry& entry) { return entry.type == type; });
    return found == m_storage->entries.end() ? nullptr : found->system.get();
}

const SceneSystem* SceneSystemManager::findSystem(std::type_index type) const noexcept {
    const auto found = std::find_if(m_storage->entries.cbegin(), m_storage->entries.cend(),
            [type](const Storage::Entry& entry) { return entry.type == type; });
    return found == m_storage->entries.cend() ? nullptr : found->system.get();
}

bool SceneSystemManager::removeSystem(std::type_index type) {
    if (m_storage->executing || m_storage->inLifecycleCallback) {
        getLogChannel().warn(
                "Rejected removal of System '{}' during System execution or a lifecycle callback.",
                type.name());
        throw std::logic_error(
                "Systems cannot be removed during System execution or lifecycle callbacks.");
    }

    const auto found = std::find_if(m_storage->entries.begin(), m_storage->entries.end(),
            [type](const Storage::Entry& entry) { return entry.type == type; });
    if (found == m_storage->entries.end()) {
        return false;
    }

    m_storage->inLifecycleCallback = true;
    try {
        found->system->onDetach(m_scene);
    } catch (const std::exception& exception) {
        logCallbackFailure(type, "onDetach", exception);
        m_storage->inLifecycleCallback = false;
        throw;
    } catch (...) {
        logUnknownCallbackFailure(type, "onDetach");
        m_storage->inLifecycleCallback = false;
        throw;
    }
    m_storage->inLifecycleCallback = false;
    m_storage->entries.erase(found);
    getLogChannel().debug("Removed system '{}'", type.name());
    return true;
}

void SceneSystemManager::setSystemEnabled(std::type_index type, bool enabled) {
    const auto found = std::find_if(m_storage->entries.begin(), m_storage->entries.end(),
            [type](const Storage::Entry& entry) { return entry.type == type; });
    if (found == m_storage->entries.end()) {
        throw std::out_of_range("The requested System is not registered with this Scene.");
    }
    if (found->enabled == enabled) {
        return;
    }
    found->enabled = enabled;
    getLogChannel().debug("{} system '{}'", enabled ? "Enabled" : "Disabled", type.name());
}

bool SceneSystemManager::isSystemEnabled(std::type_index type) const noexcept {
    const auto found = std::find_if(m_storage->entries.cbegin(), m_storage->entries.cend(),
            [type](const Storage::Entry& entry) { return entry.type == type; });
    return found != m_storage->entries.cend() && found->enabled;
}

void SceneSystemManager::runSystems(SystemStage stage, const UpdateContext& context) {
    if (!isValidSystemStage(stage)) {
        throw std::invalid_argument("The requested SystemStage is not valid.");
    }
    if (m_storage->executing || m_storage->inLifecycleCallback) {
        getLogChannel().warn(
                "Rejected nested System execution for stage {}.", systemStageName(stage));
        throw std::logic_error("Scene System execution cannot be nested.");
    }

    m_storage->executing = true;
    try {
        m_scene.updateWorldTransforms();
        for (const Storage::Entry& entry: m_storage->entries) {
            if (entry.stage == stage && entry.enabled) {
                try {
                    entry.system->onUpdate(m_scene, context);
                } catch (const std::exception& exception) {
                    logCallbackFailure(entry.type, "onUpdate", exception);
                    throw;
                } catch (...) {
                    logUnknownCallbackFailure(entry.type, "onUpdate");
                    throw;
                }
            }
        }
    } catch (...) {
        m_storage->executing = false;
        throw;
    }
    m_storage->executing = false;
}

} // namespace arti::scene
