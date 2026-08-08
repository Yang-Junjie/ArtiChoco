#pragma once

#include "components.h"
#include "serialization.h"

#include <cstdint>

#include <entt/entt.hpp>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arti::scene {

class SceneSerializer;

class SceneSerializationRegistry {
public:
    SceneSerializationRegistry();

    SceneSerializationRegistry(const SceneSerializationRegistry&) = delete;
    SceneSerializationRegistry& operator=(const SceneSerializationRegistry&) = delete;

    template <typename Component>
    void registerComponent(std::string type_name, std::unique_ptr<Serialization<Component>> serialization)
    {
        static_assert(!std::is_same_v<Component, IDComponent> && !std::is_same_v<Component, TagComponent> &&
                          !std::is_same_v<Component, ParentComponent> &&
                          !std::is_same_v<Component, WorldTransformComponent>,
                      "Scene-owned components are serialized by SceneSerializer.");

        if (type_name.empty()) {
            throw std::invalid_argument("A component serialization type name cannot be empty.");
        }
        if (!serialization) {
            throw std::invalid_argument("A component serialization cannot be null.");
        }

        registerEntry(std::make_unique<ComponentEntry<Component>>(std::move(type_name), std::move(serialization)));
    }

private:
    class Entry {
    public:
        virtual ~Entry() = default;

        virtual entt::id_type componentType() const noexcept = 0;
        virtual std::string_view typeName() const noexcept = 0;
        virtual bool contains(const entt::registry& registry, entt::entity entity) const noexcept = 0;
        virtual YAML::Node serialize(const entt::registry& registry, entt::entity entity) const = 0;
        virtual void deserialize(entt::registry& registry, entt::entity entity, const YAML::Node& node) const = 0;
    };

    template <typename Component> class ComponentEntry final : public Entry {
    public:
        ComponentEntry(std::string type_name, std::unique_ptr<Serialization<Component>> serialization)
            : m_type_name(std::move(type_name)),
              m_serialization(std::move(serialization))
        {}

        entt::id_type componentType() const noexcept override
        {
            return entt::type_hash<Component>::value();
        }

        std::string_view typeName() const noexcept override
        {
            return m_type_name;
        }

        bool contains(const entt::registry& registry, entt::entity entity) const noexcept override
        {
            return registry.all_of<Component>(entity);
        }

        YAML::Node serialize(const entt::registry& registry, entt::entity entity) const override
        {
            return m_serialization->serialize(registry.get<Component>(entity));
        }

        void deserialize(entt::registry& registry, entt::entity entity, const YAML::Node& node) const override
        {
            registry.emplace_or_replace<Component>(entity, m_serialization->deserialize(node));
        }

    private:
        std::string m_type_name;
        std::unique_ptr<Serialization<Component>> m_serialization;
    };

    void registerEntry(std::unique_ptr<Entry> entry);
    const Entry* findEntry(std::string_view type_name) const noexcept;

    const std::vector<std::unique_ptr<Entry>>& entries() const noexcept
    {
        return m_entries;
    }

    std::vector<std::unique_ptr<Entry>> m_entries;
    std::unordered_map<std::string, Entry*> m_by_name;
    std::unordered_map<entt::id_type, Entry*> m_by_component_type;

    friend class SceneSerializer;
};

} // namespace arti::scene
