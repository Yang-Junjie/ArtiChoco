#pragma once
#include "artichoco/scene/component/component_serialization.h"
#include "artichoco/scene/components.h"

#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>

#include <stdexcept>
#include <string>

namespace arti::scene {

class TransformSerialization final : public ComponentSerialization<TransformComponent> {
public:
    static constexpr std::string_view typeName() noexcept { return "arti.transform"; }

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

private:
    static YAML::Node writeVector3(const glm::vec3& value)
    {
        YAML::Node node(YAML::NodeType::Sequence);
        node.push_back(value.x);
        node.push_back(value.y);
        node.push_back(value.z);
        node.SetStyle(YAML::EmitterStyle::Flow);
        return node;
    }

    static glm::vec3 readVector3(const YAML::Node& node, const char* field)
    {
        const YAML::Node value = node[field];
        if (!value || !value.IsSequence() || value.size() != 3) {
            throw std::invalid_argument(std::string("Transform field '") + field + "' must contain three values.");
        }
        return glm::vec3{value[0].as<float>(), value[1].as<float>(), value[2].as<float>()};
    }

    static YAML::Node writeQuaternion(const glm::quat& value)
    {
        YAML::Node node(YAML::NodeType::Sequence);
        node.push_back(value.w);
        node.push_back(value.x);
        node.push_back(value.y);
        node.push_back(value.z);
        node.SetStyle(YAML::EmitterStyle::Flow);
        return node;
    }

    static glm::quat readQuaternion(const YAML::Node& node)
    {
        const YAML::Node value = node["Rotation"];
        if (!value || !value.IsSequence() || value.size() != 4) {
            throw std::invalid_argument("Transform field 'Rotation' must contain four values.");
        }
        return glm::quat{value[0].as<float>(), value[1].as<float>(), value[2].as<float>(), value[3].as<float>()};
    }
};

} // namespace arti::scene
