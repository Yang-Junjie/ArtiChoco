#include "hello_triangle_layer.h"

#include "application.h"
#include "artichoco/platform/window/sdl_vulkan_surface_source.h"
#include "artichoco/renderer/render_device.h"
#include "triangle_pass.h"

#include <array>
#include <filesystem>
#include <span>
#include <utility>

namespace arti::hello_triangle {

HelloTriangleLayer::HelloTriangleLayer(bool smoke)
    : Layer("HelloTriangleLayer"),
      m_smoke(smoke),
      m_smoke_frames_remaining(smoke ? 3u : 0u)
{}

HelloTriangleLayer::~HelloTriangleLayer() = default;

void HelloTriangleLayer::onAttach()
{
    auto& application = core::Application::get();
    auto surface_source = platform::createSDLVulkanSurfaceSource(application.getWindow());
    renderer::RenderDeviceCreateInfo device_info;
    device_info.application_name = "Hello Triangle";

    m_render_device = std::make_unique<renderer::RenderDevice>(application.getWindow(),
            std::move(surface_source), device_info);
    m_triangle_pass = std::make_unique<TrianglePass>(
            *m_render_device, std::filesystem::path{ARTI_HELLO_TRIANGLE_SHADER_PATH});
    application.getLogChannel().info("Hello Triangle NVRHI renderer initialized");
}

void HelloTriangleLayer::onDetach()
{
    if (m_render_device) {
        m_render_device->waitIdle();
    }

    m_triangle_pass.reset();
    m_render_device.reset();
}

void HelloTriangleLayer::onRender()
{
    if (!m_render_device || !m_triangle_pass) {
        return;
    }

    renderer::RenderPass* pass = m_triangle_pass.get();
    const std::array passes = {pass};
    if (!m_render_device->renderFrame(
                std::span<renderer::RenderPass* const>{passes.data(), passes.size()})) {
        return;
    }

    if (m_smoke && --m_smoke_frames_remaining == 0) {
        core::Application::get().getLogChannel().info(
                "Hello Triangle NVRHI smoke test passed");
        core::Application::get().close();
    }
}

} // namespace arti::hello_triangle
