#pragma once

#include <yaml-cpp/yaml.h>

namespace arti::scene {

template <typename Component> class Serialization {
public:
    virtual ~Serialization() = default;

    virtual YAML::Node serialize(const Component& component) const = 0;
    virtual Component deserialize(const YAML::Node& node) const = 0;
};

} // namespace arti::scene
