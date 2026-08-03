#include "application.h"
#include "artichoco/platform/window/sdl_vulkan_surface_source.h"
#include "artichoco/renderer/vulkan/vulkan_allocator.h"
#include "artichoco/renderer/vulkan/vulkan_context.h"
#include "artichoco/renderer/vulkan/vulkan_device.h"
#include "artichoco/renderer/vulkan/vulkan_surface.h"
#include "artichoco/renderer/renderer.h"
#include "test_app_layer.h"

#include <array>
#include <cstring>

namespace arti::test_app {

TestAppLayer::TestAppLayer(bool smoke_vulkan, bool smoke_render)
    : Layer("TestAppLayer"),
      m_smoke_vulkan(smoke_vulkan),
      m_smoke_render(smoke_render)
{}

TestAppLayer::~TestAppLayer() = default;

void TestAppLayer::onAttach()
{
    core::Application::get().getLogChannel().info("hello world");

    if (m_smoke_render) {
        auto surface_source = platform::createSDLVulkanSurfaceSource(core::Application::get().getWindow());
        renderer::RendererCreateInfo renderer_info;
        renderer_info.application_name = "Test App";
        renderer_info.shader_path = ARTI_TEST_TRIANGLE_SHADER_PATH;
        m_renderer = std::make_unique<renderer::Renderer>(
            core::Application::get().getWindow(), std::move(surface_source), renderer_info);
        m_render_frames_remaining = 5;
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
    buffer_info.setSize(sizeof(values)).setUsage(vk::BufferUsageFlagBits::eTransferSrc).setSharingMode(vk::SharingMode::eExclusive);

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
    m_renderer.reset();
}

void TestAppLayer::onRender()
{
    if (!m_renderer || !m_renderer->renderFrame()) {
        return;
    }

    if (m_render_frames_remaining == 5) {
        core::Application::get().getWindow().resize(960, 540);
    }

    if (--m_render_frames_remaining == 0) {
        core::Application::get().getLogChannel().info("Vulkan triangle smoke test passed");
        core::Application::get().close();
    }
}
} // namespace arti::test_app
