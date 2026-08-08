#include "scene.h"
#include "scene_log.h"
#include "scene_serializer.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace arti::scene {
namespace {

core::UUID readUUID(const YAML::Node& node, const char* field)
{
    if (!node || !node.IsScalar()) {
        throw std::invalid_argument(std::string("Scene field '") + field + "' must be a UUID string.");
    }

    const auto parsed = core::UUID::fromString(node.as<std::string>());
    if (!parsed || !parsed->isValid()) {
        throw std::invalid_argument(std::string("Scene field '") + field + "' contains an invalid UUID.");
    }
    return *parsed;
}

std::string readTag(const YAML::Node& node)
{
    if (!node || !node.IsScalar()) {
        throw std::invalid_argument("Scene field 'Tag' must be a string.");
    }
    return node.as<std::string>();
}

} // namespace

SceneSerializer::SceneSerializer(const SceneSerializationRegistry& registry) noexcept
    : m_registry(registry)
{}

YAML::Node SceneSerializer::serialize(const Scene& scene) const
{
    YAML::Node root;
    YAML::Node entities(YAML::NodeType::Sequence);

    std::vector<entt::entity> entity_handles;
    const auto* entity_storage = scene.m_registry.storage<entt::entity>();
    entity_handles.reserve(entity_storage->size());
    for (auto [entity] : entity_storage->each()) {
        entity_handles.push_back(entity);
    }
    std::ranges::sort(entity_handles, [&scene](entt::entity lhs, entt::entity rhs) {
        return scene.m_registry.get<IDComponent>(lhs).id.value() < scene.m_registry.get<IDComponent>(rhs).id.value();
    });

    std::vector<const SceneSerializationRegistry::Entry*> serialization_entries;
    serialization_entries.reserve(m_registry.entries().size());
    for (const auto& entry : m_registry.entries()) {
        serialization_entries.push_back(entry.get());
    }
    std::ranges::sort(serialization_entries, [](const auto* lhs, const auto* rhs) {
        return lhs->typeName() < rhs->typeName();
    });

    for (const entt::entity entity : entity_handles) {
        const auto& id = scene.m_registry.get<IDComponent>(entity);
        const auto& tag = scene.m_registry.get<TagComponent>(entity);
        const auto& parent = scene.m_registry.get<ParentComponent>(entity);

        YAML::Node entity_node;
        entity_node["ID"] = id.id.toString();
        entity_node["Tag"] = tag.tag;
        if (parent.parent_id.isValid()) {
            entity_node["Parent"] = parent.parent_id.toString();
        } else {
            entity_node["Parent"] = YAML::Null;
        }

        YAML::Node components(YAML::NodeType::Map);
        for (const SceneSerializationRegistry::Entry* entry : serialization_entries) {
            if (entry->contains(scene.m_registry, entity)) {
                components[std::string(entry->typeName())] = entry->serialize(scene.m_registry, entity);
            }
        }
        entity_node["Components"] = components;
        entities.push_back(entity_node);
    }

    root["Entities"] = entities;
    return root;
}

void SceneSerializer::deserialize(const YAML::Node& node, Scene& scene) const
{
    if (!node || !node.IsMap()) {
        throw std::invalid_argument("Scene YAML root must be a map.");
    }

    const YAML::Node entities_node = node["Entities"];
    if (!entities_node || !entities_node.IsSequence()) {
        throw std::invalid_argument("Scene field 'Entities' must be a sequence.");
    }

    struct PendingComponent {
        const SceneSerializationRegistry::Entry* serialization{nullptr};
        YAML::Node node;
    };

    struct PendingEntity {
        core::UUID id;
        std::string tag;
        core::UUID parent_id;
        std::vector<PendingComponent> components;
    };

    std::vector<PendingEntity> pending_entities;
    pending_entities.reserve(entities_node.size());
    std::unordered_map<core::UUID, size_t> entity_indices;

    for (const YAML::Node& entity_node : entities_node) {
        if (!entity_node.IsMap()) {
            throw std::invalid_argument("Each serialized Entity must be a YAML map.");
        }

        PendingEntity pending;
        pending.id = readUUID(entity_node["ID"], "ID");
        pending.tag = readTag(entity_node["Tag"]);

        const YAML::Node parent_node = entity_node["Parent"];
        if (parent_node && !parent_node.IsNull()) {
            pending.parent_id = readUUID(parent_node, "Parent");
        }

        const auto [_, inserted] = entity_indices.emplace(pending.id, pending_entities.size());
        if (!inserted) {
            throw std::invalid_argument("Scene YAML contains a duplicate Entity UUID.");
        }

        const YAML::Node components_node = entity_node["Components"];
        if (components_node) {
            if (!components_node.IsMap()) {
                throw std::invalid_argument("Scene field 'Components' must be a map.");
            }

            std::unordered_set<std::string> component_types;
            for (const auto& component_node : components_node) {
                if (!component_node.first.IsScalar()) {
                    throw std::invalid_argument("A serialized component type name must be a string.");
                }
                std::string type_name = component_node.first.as<std::string>();
                if (!component_types.insert(type_name).second) {
                    throw std::invalid_argument("An Entity contains a duplicate serialized component type.");
                }

                const auto* serialization = m_registry.findEntry(type_name);
                if (serialization == nullptr) {
                    throw std::invalid_argument("Scene YAML contains an unregistered component type: " + type_name);
                }
                pending.components.push_back(PendingComponent{serialization, component_node.second});
            }
        }

        pending_entities.push_back(std::move(pending));
    }

    for (const PendingEntity& pending : pending_entities) {
        if (pending.parent_id.isValid() && !entity_indices.contains(pending.parent_id)) {
            throw std::invalid_argument("Scene YAML references a missing parent Entity.");
        }
    }

    enum class VisitState : uint8_t {
        Unvisited,
        Visiting,
        Visited,
    };
    std::vector<VisitState> visit_states(pending_entities.size(), VisitState::Unvisited);
    std::function<void(size_t)> validate_parent = [&](size_t index) {
        if (visit_states[index] == VisitState::Visited) {
            return;
        }
        if (visit_states[index] == VisitState::Visiting) {
            throw std::invalid_argument("Scene YAML contains a parent hierarchy cycle.");
        }

        visit_states[index] = VisitState::Visiting;
        const core::UUID parent_id = pending_entities[index].parent_id;
        if (parent_id.isValid()) {
            validate_parent(entity_indices.at(parent_id));
        }
        visit_states[index] = VisitState::Visited;
    };
    for (size_t index = 0; index < pending_entities.size(); ++index) {
        validate_parent(index);
    }

    Scene staging;
    for (const PendingEntity& pending : pending_entities) {
        staging.createEntityWithUUID(pending.id, pending.tag);
    }
    for (const PendingEntity& pending : pending_entities) {
        const entt::entity entity = staging.m_entity_lookup.at(pending.id);
        for (const PendingComponent& component : pending.components) {
            component.serialization->deserialize(staging.m_registry, entity, component.node);
        }
    }
    for (const PendingEntity& pending : pending_entities) {
        if (pending.parent_id.isValid()) {
            staging.setParent(staging.findEntity(pending.id), staging.findEntity(pending.parent_id));
        }
    }
    staging.updateWorldTransforms();

    scene.clearEntities();
    scene.m_registry = std::move(staging.m_registry);
    scene.m_entity_lookup = std::move(staging.m_entity_lookup);
}

void SceneSerializer::save(const Scene& scene, const std::filesystem::path& path) const
{
    YAML::Node root = serialize(scene);
    YAML::Emitter emitter;
    emitter << root;
    if (!emitter.good()) {
        throw std::runtime_error(std::string{"Failed to emit Scene YAML: "} + emitter.GetLastError());
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Failed to open Scene file for writing: " + path.string());
    }
    output << emitter.c_str();
    if (!output) {
        throw std::runtime_error("Failed to write Scene file: " + path.string());
    }
    getLogChannel().info("Saved scene to '{}' ({} entities)", path.string(), root["Entities"].size());
}

void SceneSerializer::load(const std::filesystem::path& path, Scene& scene) const
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open Scene file for reading: " + path.string());
    }

    YAML::Node node;
    try {
        node = YAML::Load(input);
    } catch (const YAML::Exception& exception) {
        throw std::runtime_error("Failed to parse Scene YAML '" + path.string() + "': " + exception.what());
    }
    deserialize(node, scene);
    getLogChannel().info("Loaded scene from '{}' ({} entities)", path.string(), node["Entities"].size());
}

} // namespace arti::scene
