#pragma once
#include "artichoco/core/uuid.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <utility>

namespace arti::scene {
struct IDComponent {
    core::UUID id;

    IDComponent() = default;

    explicit IDComponent(core::UUID id)
            : id(id) {}
};

struct TagComponent {
    std::string tag;

    TagComponent() = default;

    explicit TagComponent(std::string tag)
            : tag(std::move(tag)) {}
};

struct TransformComponent {
    glm::vec3 translation{ 0.0f };
    glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 scale{ 1.0f };

    glm::mat4 getTransform() const {
        glm::mat4 transform{ 1.0f };
        transform = glm::translate(transform, translation);
        transform *= glm::mat4_cast(rotation);
        transform = glm::scale(transform, scale);
        return transform;
    }
};

struct ParentComponent {
    core::UUID parent_id{};

    ParentComponent() = default;

    explicit ParentComponent(core::UUID parent_id)
            : parent_id(parent_id) {}
};

struct WorldTransformComponent {
    glm::mat4 world{ 1.0f };
    glm::mat4 local{ 1.0f };
    core::UUID parent_id{};
    bool dirty{ true };
};
} // namespace arti::scene
