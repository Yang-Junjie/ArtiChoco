#pragma once

#include "scene_serialization_registry.h"

#include <filesystem>

namespace arti::scene {

class Scene;

class SceneSerializer {
public:
    explicit SceneSerializer(const SceneSerializationRegistry& registry) noexcept;

    YAML::Node serialize(const Scene& scene) const;
    void deserialize(const YAML::Node& node, Scene& scene) const;

    void save(const Scene& scene, const std::filesystem::path& path) const;
    void load(const std::filesystem::path& path, Scene& scene) const;

private:
    const SceneSerializationRegistry& m_registry;
};

} // namespace arti::scene
