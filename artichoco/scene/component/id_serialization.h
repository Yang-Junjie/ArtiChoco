#pragma once
#include "artichoco/scene/component/component_serialization.h"
#include "artichoco/scene/components.h"

#include <yaml-cpp/yaml.h>

#include <stdexcept>
#include <string>

namespace arti::scene {

class IDSerialization final : public ComponentSerialization<IDComponent> {
public:
    static constexpr std::string_view typeName() noexcept { return "arti.id"; }

    YAML::Node serialize(const IDComponent& component) const override
    {
        return YAML::Node(component.id.toString());
    }

    IDComponent deserialize(const YAML::Node& node) const override
    {
        if (!node || !node.IsScalar()) {
            throw std::invalid_argument("Entity ID must be a UUID string.");
        }
        const auto parsed = core::UUID::fromString(node.as<std::string>());
        if (!parsed || !parsed->isValid()) {
            throw std::invalid_argument("Entity ID contains an invalid UUID.");
        }
        return IDComponent{ *parsed };
    }
};

} // namespace arti::scene
