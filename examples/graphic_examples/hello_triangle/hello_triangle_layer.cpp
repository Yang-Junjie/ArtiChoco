#include "hello_triangle_layer.h"

#include "application.h"
#include "artichoco/platform/window/sdl_vulkan_surface_source.h"
#include "artichoco/renderer/render_device.h"
#include "triangle_renderer.h"

#include <filesystem>
#include <utility>

namespace arti::hello_triangle {

HelloTriangleLayer::HelloTriangleLayer()
        : Layer("HelloTriangleLayer") {}

HelloTriangleLayer::~HelloTriangleLayer() = default;

void HelloTriangleLayer::onAttach() {
    auto& application = core::Application::get();
    application.getLogChannel().info("Creating Hello Triangle renderer");

    auto surface_source = platform::createSDLVulkanSurfaceSource(application.getWindow());
    renderer::RenderDeviceCreateInfo device_info;
    device_info.application_name = "Hello Triangle";

    m_render_device = std::make_unique<renderer::RenderDevice>(application.getWindow(),
            std::move(surface_source), device_info);
    m_renderer = std::make_unique<TriangleRenderer>(*m_render_device,
            std::filesystem::path{ ARTI_HELLO_TRIANGLE_SHADER_PATH });
}

void HelloTriangleLayer::onDetach() {
    if (m_render_device) {
        m_render_device->waitIdle();
    }

    m_renderer.reset();
    m_render_device.reset();
}

void HelloTriangleLayer::onRender() {
    if (m_renderer) {
        m_renderer->renderFrame();
    }
}

} // namespace arti::hello_triangle
