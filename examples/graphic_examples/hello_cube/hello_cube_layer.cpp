#include "hello_cube_layer.h"

#include "application.h"
#include "artichoco/platform/window/sdl_vulkan_surface_source.h"
#include "artichoco/renderer/render_device.h"
#include "cube_pass.h"
#include "image_loader.h"

#include <array>
#include <filesystem>
#include <span>
#include <utility>

namespace arti::hello_cube {

HelloCubeLayer::HelloCubeLayer(bool smoke)
        : Layer("HelloCubeLayer"),
          m_smoke(smoke),
          m_smoke_frames_remaining(smoke ? 5u : 0u) {}

HelloCubeLayer::~HelloCubeLayer() = default;

void HelloCubeLayer::onAttach() {
    auto& application = core::Application::get();
    auto surface_source = platform::createSDLVulkanSurfaceSource(application.getWindow());
    renderer::RenderDeviceCreateInfo device_info;
    device_info.application_name = "Hello Cube";
    m_render_device = std::make_unique<renderer::RenderDevice>(application.getWindow(),
            std::move(surface_source), device_info);

    const ImageData image = loadImageRGBA(ARTI_HELLO_CUBE_IMAGE_PATH);
    auto texture = m_render_device->createTexture2D(image.rgba_pixels, image.width, image.height,
            renderer::TextureFormat::RGBA8Srgb, true);
    m_cube_pass = std::make_unique<CubePass>(*m_render_device, std::move(texture),
            std::filesystem::path{ ARTI_HELLO_CUBE_SHADER_PATH });
    application.getLogChannel().info("Hello Cube initialized with image.png ({}x{})", image.width,
            image.height);
}

void HelloCubeLayer::onDetach() {
    if (m_render_device) {
        m_render_device->waitIdle();
    }
    m_cube_pass.reset();
    m_render_device.reset();
}

void HelloCubeLayer::onUpdate(core::Timestep delta_time) {
    m_rotation += delta_time.getSeconds() * 0.8f;
    if (m_cube_pass) {
        m_cube_pass->setRotation(m_rotation);
    }
}

void HelloCubeLayer::onRender() {
    if (!m_render_device || !m_cube_pass) {
        return;
    }

    renderer::RenderPass* pass = m_cube_pass.get();
    const std::array passes = { pass };
    if (!m_render_device->renderFrame(
                std::span<renderer::RenderPass* const>{ passes.data(), passes.size() })) {
        return;
    }

    if (m_smoke && --m_smoke_frames_remaining == 0) {
        core::Application::get().getLogChannel().info(
                "Hello Cube textured NVRHI smoke test passed");
        core::Application::get().close();
    }
}

} // namespace arti::hello_cube
