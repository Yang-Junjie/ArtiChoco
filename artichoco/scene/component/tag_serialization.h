#pragma once
#include "artichoco/scene/component/component_serialization.h"
#include "artichoco/scene/components.h"

#include <yaml-cpp/yaml.h>

#include <stdexcept>
#include <string>

namespace arti::scene {

class TagSerialization final : public ComponentSerialization<TagComponent> {
public:
    static constexpr std::string_view typeName() noexcept { return "arti.tag"; }

    YAML::Node serialize(const TagComponent& component) const override
    {
        return YAML::Node(component.tag);
    }

    TagComponent deserialize(const YAML::Node& node) const override
    {
        if (!node || !node.IsScalar()) {
            throw std::invalid_argument("Entity Tag must be a string.");
        }
        return TagComponent{ node.as<std::string>() };
    }
};

} // namespace arti::scene
