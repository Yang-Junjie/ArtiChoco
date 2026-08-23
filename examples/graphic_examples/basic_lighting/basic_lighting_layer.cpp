#include "basic_lighting_layer.h"

#include "application.h"
#include "artichoco/platform/window/sdl_vulkan_surface_source.h"
#include "artichoco/renderer/render_device.h"
#include "lighting_pass.h"
#include "present_pass.h"

#include <array>
#include <filesystem>
#include <span>
#include <utility>

namespace arti::basic_lighting {

BasicLightingLayer::BasicLightingLayer(bool smoke)
        : Layer("BasicLightingLayer"),
          m_smoke(smoke),
          m_smoke_frames_remaining(smoke ? 6u : 0u) {}

BasicLightingLayer::~BasicLightingLayer() = default;

void BasicLightingLayer::onAttach() {
    auto& application = core::Application::get();
    auto surface_source = platform::createSDLVulkanSurfaceSource(application.getWindow());
    renderer::RenderDeviceCreateInfo device_info;
    device_info.application_name = "Basic Lighting";
    m_render_device = std::make_unique<renderer::RenderDevice>(application.getWindow(),
            std::move(surface_source), device_info);
    m_lighting_pass = std::make_unique<LightingPass>(*m_render_device,
            std::filesystem::path{ ARTI_BASIC_LIGHTING_SHADER_PATH });
    m_present_pass = std::make_unique<PresentPass>(*m_lighting_pass,
            std::filesystem::path{ ARTI_BASIC_LIGHTING_PRESENT_SHADER_PATH });
    application.getLogChannel().info("Basic Lighting NVRHI renderer initialized");
}

void BasicLightingLayer::onDetach() {
    if (m_render_device) {
        m_render_device->waitIdle();
    }
    m_present_pass.reset();
    m_lighting_pass.reset();
    m_render_device.reset();
}

void BasicLightingLayer::onUpdate(core::Timestep delta_time) {
    m_rotation += delta_time.getSeconds() * 0.65f;
    if (m_lighting_pass) {
        m_lighting_pass->setRotation(m_rotation);
    }
}

void BasicLightingLayer::onRender() {
    if (!m_render_device || !m_lighting_pass || !m_present_pass) {
        return;
    }

    renderer::RenderPass* lighting_pass = m_lighting_pass.get();
    renderer::RenderPass* present_pass = m_present_pass.get();
    const std::array passes = { lighting_pass, present_pass };
    if (!m_render_device->renderFrame(
                std::span<renderer::RenderPass* const>{ passes.data(), passes.size() })) {
        return;
    }

    if (m_smoke && m_smoke_frames_remaining == 5) {
        core::Application::get().getWindow().resize(960, 540);
    }
    if (m_smoke && --m_smoke_frames_remaining == 0) {
        core::Application::get().getLogChannel().info(
                "Basic Lighting depth/resize smoke test passed");
        core::Application::get().close();
    }
}

} // namespace arti::basic_lighting
