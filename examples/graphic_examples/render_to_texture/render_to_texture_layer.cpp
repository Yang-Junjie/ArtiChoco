#include "render_to_texture_layer.h"

#include "application.h"
#include "artichoco/platform/window/sdl_vulkan_surface_source.h"
#include "artichoco/renderer/render_device.h"
#include "display_pass.h"
#include "render_texture_pass.h"

#include <array>
#include <filesystem>
#include <span>
#include <utility>

namespace arti::render_to_texture {

RenderToTextureLayer::RenderToTextureLayer(bool smoke)
        : Layer("RenderToTextureLayer"),
          m_smoke(smoke),
          m_smoke_frames_remaining(smoke ? 6u : 0u) {}

RenderToTextureLayer::~RenderToTextureLayer() = default;

void RenderToTextureLayer::onAttach() {
    auto& application = core::Application::get();
    auto surface_source = platform::createSDLVulkanSurfaceSource(application.getWindow());
    renderer::RenderDeviceCreateInfo device_info;
    device_info.application_name = "Render To Texture";
    m_render_device = std::make_unique<renderer::RenderDevice>(application.getWindow(),
            std::move(surface_source), device_info);
    m_render_texture_pass = std::make_unique<RenderTexturePass>(
            std::filesystem::path{ ARTI_RENDER_TO_TEXTURE_SHADER_PATH });
    m_display_pass = std::make_unique<DisplayPass>(*m_render_device, *m_render_texture_pass,
            std::filesystem::path{ ARTI_RENDER_TO_TEXTURE_DISPLAY_SHADER_PATH });
    application.getLogChannel().info(
            "Render To Texture NVRHI renderer initialized (512x512 target)");
}

void RenderToTextureLayer::onDetach() {
    if (m_render_device) {
        m_render_device->waitIdle();
    }
    m_display_pass.reset();
    m_render_texture_pass.reset();
    m_render_device.reset();
}

void RenderToTextureLayer::onUpdate(core::Timestep delta_time) {
    m_time += delta_time.getSeconds();
    if (m_render_texture_pass) {
        m_render_texture_pass->setTime(m_time);
    }
    if (m_display_pass) {
        m_display_pass->setRotation(m_time * 0.62f);
    }
}

void RenderToTextureLayer::onRender() {
    if (!m_render_device || !m_render_texture_pass || !m_display_pass) {
        return;
    }

    renderer::RenderPass* render_texture_pass = m_render_texture_pass.get();
    renderer::RenderPass* display_pass = m_display_pass.get();
    const std::array passes = { render_texture_pass, display_pass };
    if (!m_render_device->renderFrame(
                std::span<renderer::RenderPass* const>{ passes.data(), passes.size() })) {
        return;
    }

    if (m_smoke && m_smoke_frames_remaining == 5) {
        core::Application::get().getWindow().resize(960, 540);
    }
    if (m_smoke && --m_smoke_frames_remaining == 0) {
        core::Application::get().getLogChannel().info(
                "Render To Texture transition/depth/resize smoke test passed");
        core::Application::get().close();
    }
}

} // namespace arti::render_to_texture
