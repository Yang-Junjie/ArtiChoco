#pragma once
#include "system.h"

#include <concepts>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <utility>

namespace arti::scene {

class Scene;

class SceneSystemManager {
public:
    explicit SceneSystemManager(Scene& scene);
    ~SceneSystemManager();

    SceneSystemManager(const SceneSystemManager&) = delete;
    SceneSystemManager& operator=(const SceneSystemManager&) = delete;
    SceneSystemManager(SceneSystemManager&&) = delete;
    SceneSystemManager& operator=(SceneSystemManager&&) = delete;

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
    struct Storage;

    SceneSystem& registerSystem(SystemStage stage, std::type_index type,
            std::unique_ptr<SceneSystem> system);
    SceneSystem* findSystem(std::type_index type) noexcept;
    const SceneSystem* findSystem(std::type_index type) const noexcept;
    bool removeSystem(std::type_index type);
    void setSystemEnabled(std::type_index type, bool enabled);
    bool isSystemEnabled(std::type_index type) const noexcept;

    Scene& m_scene;
    std::unique_ptr<Storage> m_storage;
};

} // namespace arti::scene
