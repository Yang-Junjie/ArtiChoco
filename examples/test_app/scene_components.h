#pragma once
#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/texture_2d.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/scene/components.h"

#include <glm/glm.hpp>
#include <memory>
#include <utility>

namespace arti::test_app {

class Mesh {
public:
    Mesh(renderer::VertexBuffer vertex_buffer, renderer::IndexBuffer index_buffer)
        : m_vertex_buffer(std::make_shared<renderer::VertexBuffer>(std::move(vertex_buffer))),
          m_index_buffer(std::make_shared<renderer::IndexBuffer>(std::move(index_buffer)))
    {}

    const renderer::VertexBuffer& vertexBuffer() const noexcept
    {
        return *m_vertex_buffer;
    }

    const renderer::IndexBuffer& indexBuffer() const noexcept
    {
        return *m_index_buffer;
    }

    const std::shared_ptr<renderer::VertexBuffer>& vertexBufferPtr() const noexcept
    {
        return m_vertex_buffer;
    }

    const std::shared_ptr<renderer::IndexBuffer>& indexBufferPtr() const noexcept
    {
        return m_index_buffer;
    }

    bool sharesBuffersWith(const Mesh& other) const noexcept
    {
        return m_vertex_buffer == other.m_vertex_buffer && m_index_buffer == other.m_index_buffer;
    }

private:
    std::shared_ptr<renderer::VertexBuffer> m_vertex_buffer;
    std::shared_ptr<renderer::IndexBuffer> m_index_buffer;
};

class Material {
public:
    explicit Material(renderer::Texture2D base_color_texture)
        : m_base_color_texture(std::make_shared<renderer::Texture2D>(std::move(base_color_texture)))
    {}

    const renderer::Texture2D& baseColorTexture() const noexcept
    {
        return *m_base_color_texture;
    }

    const std::shared_ptr<renderer::Texture2D>& texturePtr() const noexcept
    {
        return m_base_color_texture;
    }

    bool sharesTextureWith(const Material& other) const noexcept
    {
        return m_base_color_texture == other.m_base_color_texture;
    }

private:
    std::shared_ptr<renderer::Texture2D> m_base_color_texture;
};

struct MeshComponent {
    Mesh mesh;
};

struct MaterialComponent {
    Material material;
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
