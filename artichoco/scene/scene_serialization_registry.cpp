#include "scene_serialization_registry.h"
#include "scene_log.h"

#include <stdexcept>
#include <utility>

namespace arti::scene {
namespace {

YAML::Node writeVector3(const glm::vec3& value)
{
    YAML::Node node(YAML::NodeType::Sequence);
    node.push_back(value.x);
    node.push_back(value.y);
    node.push_back(value.z);
    return node;
}

glm::vec3 readVector3(const YAML::Node& node, const char* field)
{
    const YAML::Node value = node[field];
    if (!value || !value.IsSequence() || value.size() != 3) {
        throw std::invalid_argument(std::string("Transform field '") + field + "' must contain three values.");
    }
    return glm::vec3{value[0].as<float>(), value[1].as<float>(), value[2].as<float>()};
}

YAML::Node writeQuaternion(const glm::quat& value)
{
    YAML::Node node(YAML::NodeType::Sequence);
    node.push_back(value.w);
    node.push_back(value.x);
    node.push_back(value.y);
    node.push_back(value.z);
    return node;
}

glm::quat readQuaternion(const YAML::Node& node)
{
    const YAML::Node value = node["Rotation"];
    if (!value || !value.IsSequence() || value.size() != 4) {
        throw std::invalid_argument("Transform field 'Rotation' must contain four values.");
    }
    return glm::quat{value[0].as<float>(), value[1].as<float>(), value[2].as<float>(), value[3].as<float>()};
}

class TransformSerialization final : public Serialization<TransformComponent> {
public:
    YAML::Node serialize(const TransformComponent& component) const override
    {
        YAML::Node node;
        node["Translation"] = writeVector3(component.translation);
        node["Rotation"] = writeQuaternion(component.rotation);
        node["Scale"] = writeVector3(component.scale);
        return node;
    }

    TransformComponent deserialize(const YAML::Node& node) const override
    {
        if (!node || !node.IsMap()) {
            throw std::invalid_argument("Transform component data must be a YAML map.");
        }

        TransformComponent component;
        component.translation = readVector3(node, "Translation");
        component.rotation = readQuaternion(node);
        component.scale = readVector3(node, "Scale");
        return component;
    }
};

} // namespace

SceneSerializationRegistry::SceneSerializationRegistry()
{
    registerComponent<TransformComponent>("arti.transform", std::make_unique<TransformSerialization>());
}

void SceneSerializationRegistry::registerEntry(std::unique_ptr<Entry> entry)
{
    if (!entry) {
        throw std::invalid_argument("A component serialization entry cannot be null.");
    }
    const std::string type_name{entry->typeName()};
    if (m_by_name.contains(type_name)) {
        getLogChannel().warn("Rejected duplicate component serialization type name: {}", type_name);
        throw std::logic_error("A component serialization type name is already registered.");
    }
    if (m_by_component_type.contains(entry->componentType())) {
        getLogChannel().warn("Rejected duplicate component serialization for type name: {}", type_name);
        throw std::logic_error("A component serialization type is already registered.");
    }

    Entry* raw_entry = entry.get();
    m_entries.push_back(std::move(entry));
    m_by_name.emplace(type_name, raw_entry);
    m_by_component_type.emplace(raw_entry->componentType(), raw_entry);
    getLogChannel().debug("Registered component serialization '{}'", type_name);
}

const SceneSerializationRegistry::Entry*
    SceneSerializationRegistry::findEntry(std::string_view type_name) const noexcept
{
    const auto found = m_by_name.find(std::string(type_name));
    return found == m_by_name.end() ? nullptr : found->second;
}

} // namespace arti::scene
