#include "triangle_renderer.h"

#include "artichoco/renderer/render_device.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "triangle_pass.h"

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>

namespace arti::hello_triangle {
namespace {

struct TriangleVertex {
    std::array<float, 2> position;
    std::array<float, 3> color;
};

static_assert(std::is_standard_layout_v<TriangleVertex>);

constexpr std::array triangleVertices = {
    TriangleVertex{ { -0.75f, -0.65f }, { 1.0f, 0.2f, 0.15f } },
    TriangleVertex{ { 0.75f, -0.65f }, { 0.2f, 1.0f, 0.25f } },
    TriangleVertex{ { 0.0f, 0.75f }, { 0.2f, 0.45f, 1.0f } },
};

renderer::VertexBuffer createTriangleVertexBuffer(renderer::RenderDevice& render_device) {
    renderer::VertexBufferLayout layout;
    layout.stride = sizeof(TriangleVertex);
    layout.attributes = {
        { 0, renderer::VertexAttributeType::Float2, offsetof(TriangleVertex, position) },
        { 1, renderer::VertexAttributeType::Float3, offsetof(TriangleVertex, color) },
    };

    return render_device.createVertexBuffer(std::as_bytes(std::span{ triangleVertices }),
            static_cast<uint32_t>(triangleVertices.size()), std::move(layout));
}

} // namespace

struct TriangleRenderer::Impl {
    Impl(renderer::RenderDevice& render_device, std::filesystem::path shader_path)
            : render_device(render_device),
              triangle_pass(createTriangleVertexBuffer(render_device), std::move(shader_path)) {}

    renderer::RenderDevice& render_device;
    TrianglePass triangle_pass;
};

TriangleRenderer::TriangleRenderer(renderer::RenderDevice& render_device,
        std::filesystem::path shader_path)
        : m_impl(std::make_unique<Impl>(render_device, std::move(shader_path))) {}

TriangleRenderer::~TriangleRenderer() = default;

bool TriangleRenderer::renderFrame() {
    std::array<renderer::vulkan::VulkanPass*, 1> passes = { &m_impl->triangle_pass };
    return m_impl->render_device.renderFrame(passes);
}

} // namespace arti::hello_triangle
