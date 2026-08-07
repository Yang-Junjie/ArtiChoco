#pragma once
#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/texture_2d.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/scene/components.h"

#include <glm/glm.hpp>

namespace arti::test_app {

struct MeshComponent {
    renderer::VertexBuffer vertex_buffer;
    renderer::IndexBuffer index_buffer;
};

struct MaterialComponent {
    renderer::Texture2D base_color_texture;
};

struct RotationComponent {
    glm::vec3 axis{0.0f, 1.0f, 0.0f};
    float speed{1.0f};
};

struct CameraComponent {
    float fov{45.0f};
    float near_plane{0.1f};
    float far_plane{100.0f};
};

} // namespace arti::test_app
