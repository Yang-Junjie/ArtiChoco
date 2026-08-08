#pragma once

#include "artichoco/scene/serialization.h"
#include "scene_components.h"

#include <stdexcept>

namespace arti::test_app {

class RotationComponentSerialization final : public scene::Serialization<RotationComponent> {
public:
    YAML::Node serialize(const RotationComponent& component) const override
    {
        YAML::Node axis(YAML::NodeType::Sequence);
        axis.push_back(component.axis.x);
        axis.push_back(component.axis.y);
        axis.push_back(component.axis.z);

        YAML::Node node;
        node["Axis"] = axis;
        node["Speed"] = component.speed;
        return node;
    }

    RotationComponent deserialize(const YAML::Node& node) const override
    {
        if (!node || !node.IsMap()) {
            throw std::invalid_argument("Rotation component data must be a YAML map.");
        }

        const YAML::Node axis = node["Axis"];
        const YAML::Node speed = node["Speed"];
        if (!axis || !axis.IsSequence() || axis.size() != 3 || !speed || !speed.IsScalar()) {
            throw std::invalid_argument("Rotation component data is invalid.");
        }

        return RotationComponent{
            .axis = glm::vec3{axis[0].as<float>(), axis[1].as<float>(), axis[2].as<float>()},
            .speed = speed.as<float>(),
        };
    }
};

} // namespace arti::test_app
