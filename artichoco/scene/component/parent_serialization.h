#pragma once
#include "artichoco/scene/component/component_serialization.h"
#include "artichoco/scene/components.h"

#include <yaml-cpp/yaml.h>

#include <stdexcept>
#include <string>

namespace arti::scene {

class ParentSerialization final : public ComponentSerialization<ParentComponent> {
public:
    static constexpr std::string_view typeName() noexcept { return "arti.parent"; }

    YAML::Node serialize(const ParentComponent& component) const override
    {
        if (component.parent_id.isValid()) {
            return YAML::Node(component.parent_id.toString());
        }
        return YAML::Node(YAML::NodeType::Null);
    }

    ParentComponent deserialize(const YAML::Node& node) const override
    {
        if (!node || node.IsNull()) {
            return ParentComponent{};
        }
        if (!node.IsScalar()) {
            throw std::invalid_argument("Entity Parent must be a UUID string or null.");
        }
        const auto parsed = core::UUID::fromString(node.as<std::string>());
        if (!parsed || !parsed->isValid()) {
            throw std::invalid_argument("Entity Parent contains an invalid UUID.");
        }
        return ParentComponent{ *parsed };
    }
};

} // namespace arti::scene
