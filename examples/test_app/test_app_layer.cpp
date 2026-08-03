#include "application.h"
#include "artichoco/platform/window/sdl_vulkan_surface_source.h"
#include "artichoco/renderer/renderer.h"
#include "artichoco/renderer/vulkan/vulkan_allocator.h"
#include "artichoco/renderer/vulkan/vulkan_context.h"
#include "artichoco/renderer/vulkan/vulkan_device.h"
#include "artichoco/renderer/vulkan/vulkan_surface.h"
#include "image_loader.h"
#include "test_app_layer.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <type_traits>
#include <utility>

namespace arti::test_app {
namespace {

struct Vertex {
    glm::vec3 position;
    glm::vec2 uv;
};

static_assert(std::is_standard_layout_v<Vertex>);

constexpr std::array vertices = {
    Vertex{{-0.8f, -0.8f, 0.8f}, {0.0f, 1.0f}},
    Vertex{{0.8f, -0.8f, 0.8f}, {1.0f, 1.0f}},
    Vertex{{0.8f, 0.8f, 0.8f}, {1.0f, 0.0f}},
    Vertex{{-0.8f, 0.8f, 0.8f}, {0.0f, 0.0f}},

    Vertex{{0.8f, -0.8f, -0.8f}, {0.0f, 1.0f}},
    Vertex{{-0.8f, -0.8f, -0.8f}, {1.0f, 1.0f}},
    Vertex{{-0.8f, 0.8f, -0.8f}, {1.0f, 0.0f}},
    Vertex{{0.8f, 0.8f, -0.8f}, {0.0f, 0.0f}},

    Vertex{{0.8f, -0.8f, 0.8f}, {0.0f, 1.0f}},
    Vertex{{0.8f, -0.8f, -0.8f}, {1.0f, 1.0f}},
    Vertex{{0.8f, 0.8f, -0.8f}, {1.0f, 0.0f}},
    Vertex{{0.8f, 0.8f, 0.8f}, {0.0f, 0.0f}},

    Vertex{{-0.8f, -0.8f, -0.8f}, {0.0f, 1.0f}},
    Vertex{{-0.8f, -0.8f, 0.8f}, {1.0f, 1.0f}},
    Vertex{{-0.8f, 0.8f, 0.8f}, {1.0f, 0.0f}},
    Vertex{{-0.8f, 0.8f, -0.8f}, {0.0f, 0.0f}},

    Vertex{{-0.8f, 0.8f, 0.8f}, {0.0f, 1.0f}},
    Vertex{{0.8f, 0.8f, 0.8f}, {1.0f, 1.0f}},
    Vertex{{0.8f, 0.8f, -0.8f}, {1.0f, 0.0f}},
    Vertex{{-0.8f, 0.8f, -0.8f}, {0.0f, 0.0f}},

    Vertex{{-0.8f, -0.8f, -0.8f}, {0.0f, 1.0f}},
    Vertex{{0.8f, -0.8f, -0.8f}, {1.0f, 1.0f}},
    Vertex{{0.8f, -0.8f, 0.8f}, {1.0f, 0.0f}},
    Vertex{{-0.8f, -0.8f, 0.8f}, {0.0f, 0.0f}},
};

constexpr std::array<uint32_t, 36> indices = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4,
    8, 9, 10, 10, 11, 8,
    12, 13, 14, 14, 15, 12,
    16, 17, 18, 18, 19, 16,
    20, 21, 22, 22, 23, 20,
};

renderer::VertexBufferLayout cubeVertexLayout()
{
    renderer::VertexBufferLayout layout;
    layout.stride = sizeof(Vertex);
    layout.attributes = {
        {0, renderer::VertexAttributeType::Float3, offsetof(Vertex, position)},
        {1, renderer::VertexAttributeType::Float2, offsetof(Vertex, uv)},
    };
    return layout;
}

} // namespace

struct TestAppLayer::Mesh {
    Mesh(renderer::VertexBuffer vertices, renderer::IndexBuffer indices)
        : vertex_buffer(std::move(vertices)),
          index_buffer(std::move(indices))
    {}

    renderer::VertexBuffer vertex_buffer;
    renderer::IndexBuffer index_buffer;
};

struct TestAppLayer::Material {
    explicit Material(renderer::Texture2D texture)
        : base_color_texture(std::move(texture))
    {}

    renderer::Texture2D base_color_texture;
};

TestAppLayer::TestAppLayer(bool smoke_vulkan, bool enable_renderer, bool smoke_render)
    : Layer("TestAppLayer"),
      m_smoke_vulkan(smoke_vulkan),
      m_enable_renderer(enable_renderer),
      m_smoke_render(smoke_render)
{}

TestAppLayer::~TestAppLayer() = default;

void TestAppLayer::onAttach()
{
    core::Application::get().getLogChannel().info("hello world");

    if (m_enable_renderer) {
        auto surface_source = platform::createSDLVulkanSurfaceSource(core::Application::get().getWindow());
        renderer::RendererCreateInfo renderer_info;
        renderer_info.application_name = "Test App";
        renderer_info.shader_path = ARTI_TEST_MESH_SHADER_PATH;
        m_renderer = std::make_unique<renderer::Renderer>(
            core::Application::get().getWindow(), std::move(surface_source), renderer_info);

        auto vertex_buffer = m_renderer->createVertexBuffer(
            std::as_bytes(std::span{vertices}), static_cast<uint32_t>(vertices.size()), cubeVertexLayout());
        auto index_buffer = m_renderer->createIndexBuffer(
            std::as_bytes(std::span{indices}), static_cast<uint32_t>(indices.size()));
        m_mesh = std::make_unique<Mesh>(std::move(vertex_buffer), std::move(index_buffer));

        const ImageData brownie = loadImageRGBA(ARTI_TEST_BROWNIE_TEXTURE_PATH);
        auto texture = m_renderer->createTexture2D(
            brownie.rgba_pixels, brownie.width, brownie.height, renderer::TextureFormat::RGBA8Srgb);
        m_material = std::make_unique<Material>(std::move(texture));
        m_render_frames_remaining = m_smoke_render ? 5 : 0;
        core::Application::get().getLogChannel().info(
            "Rendering textured cube demo with brownie.png ({}x{})", brownie.width, brownie.height);
        return;
    }

    if (!m_smoke_vulkan) {
        return;
    }

    auto surface_source = platform::createSDLVulkanSurfaceSource(core::Application::get().getWindow());
    renderer::vulkan::VulkanContextCreateInfo context_info;
    context_info.application_name = "Test App";
    renderer::vulkan::VulkanContext context{context_info, *surface_source};
    renderer::vulkan::VulkanSurface surface{context.instance(), *surface_source};
    renderer::vulkan::VulkanDevice device{context.instance(), surface.handle()};
    renderer::vulkan::VulkanAllocator allocator{context, device};

    constexpr std::array<uint32_t, 4> values = {1, 2, 3, 4};
    vk::BufferCreateInfo buffer_info{};
    buffer_info.setSize(sizeof(values))
        .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
        .setSharingMode(vk::SharingMode::eExclusive);

    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    auto buffer = allocator.createBuffer(buffer_info, allocation_info);
    std::memcpy(buffer.map(), values.data(), sizeof(values));
    buffer.flush();
    buffer.unmap();

    vk::ImageCreateInfo image_info{};
    image_info.setImageType(vk::ImageType::e2D)
        .setFormat(vk::Format::eR8G8B8A8Unorm)
        .setExtent({1, 1, 1})
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);

    VmaAllocationCreateInfo image_allocation_info{};
    image_allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    auto image = allocator.createImage(image_info, image_allocation_info);
    core::Application::get().getLogChannel().info("Vulkan device and VMA smoke test passed");
}

void TestAppLayer::onDetach()
{
    m_material.reset();
    m_mesh.reset();
    m_renderer.reset();
}

void TestAppLayer::onUpdate(core::Timestep delta_time)
{
    m_elapsed_time += delta_time.getSeconds();
}

void TestAppLayer::onRender()
{
    if (!m_renderer || !m_mesh || !m_material) {
        return;
    }

    const auto& window = core::Application::get().getWindow();
    const float aspect = static_cast<float>(window.getFramebufferWidth()) /
        static_cast<float>(window.getFramebufferHeight());
    const glm::mat4 model = glm::rotate(
        glm::mat4{1.0f}, m_elapsed_time * 0.85f, glm::normalize(glm::vec3{0.7f, 1.0f, 0.35f}));
    const glm::mat4 view = glm::lookAt(
        glm::vec3{2.8f, 2.2f, 3.2f}, glm::vec3{0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    projection[1][1] *= -1.0f;
    const glm::mat4 transform = projection * view * model;

    renderer::DrawCommand draw;
    draw.vertex_buffer = &m_mesh->vertex_buffer;
    draw.index_buffer = &m_mesh->index_buffer;
    draw.base_color_texture = &m_material->base_color_texture;
    draw.index_count = m_mesh->index_buffer.indexCount();
    std::memcpy(draw.transform.data(), glm::value_ptr(transform), sizeof(transform));
    const std::array draw_commands = {draw};
    if (!m_renderer->renderFrame(draw_commands)) {
        return;
    }

    if (!m_smoke_render) {
        return;
    }
    if (m_render_frames_remaining == 5) {
        auto replacement_vertices = m_renderer->createVertexBuffer(
            std::as_bytes(std::span{vertices}), static_cast<uint32_t>(vertices.size()), cubeVertexLayout());
        auto replacement_indices = m_renderer->createIndexBuffer(
            std::as_bytes(std::span{indices}), static_cast<uint32_t>(indices.size()));
        m_mesh = std::make_unique<Mesh>(std::move(replacement_vertices), std::move(replacement_indices));

        const ImageData brownie = loadImageRGBA(ARTI_TEST_BROWNIE_TEXTURE_PATH);
        auto replacement_texture = m_renderer->createTexture2D(
            brownie.rgba_pixels, brownie.width, brownie.height, renderer::TextureFormat::RGBA8Srgb);
        m_material = std::make_unique<Material>(std::move(replacement_texture));
        core::Application::get().getWindow().resize(960, 540);
    }
    if (--m_render_frames_remaining == 0) {
        core::Application::get().getLogChannel().info("Vulkan textured cube smoke test passed");
        core::Application::get().close();
    }
}

} // namespace arti::test_app
