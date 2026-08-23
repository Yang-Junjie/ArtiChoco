#include "instancing_layer.h"

#include "application.h"
#include "artichoco/platform/window/sdl_vulkan_surface_source.h"
#include "artichoco/renderer/render_device.h"
#include "instancing_pass.h"

#include <array>
#include <filesystem>
#include <span>
#include <utility>

namespace arti::instancing {

InstancingLayer::InstancingLayer(bool smoke)
        : Layer("InstancingLayer"),
          m_smoke(smoke),
          m_smoke_frames_remaining(smoke ? 6u : 0u) {}

InstancingLayer::~InstancingLayer() = default;

void InstancingLayer::onAttach() {
    auto& application = core::Application::get();
    auto surface_source = platform::createSDLVulkanSurfaceSource(application.getWindow());
    renderer::RenderDeviceCreateInfo device_info;
    device_info.application_name = "Instancing";
    m_render_device = std::make_unique<renderer::RenderDevice>(application.getWindow(),
            std::move(surface_source), device_info);
    m_instancing_pass = std::make_unique<InstancingPass>(*m_render_device,
            std::filesystem::path{ ARTI_INSTANCING_SHADER_PATH });
    application.getLogChannel().info("Instancing NVRHI renderer initialized");
}

void InstancingLayer::onDetach() {
    if (m_render_device) {
        m_render_device->waitIdle();
    }
    m_instancing_pass.reset();
    m_render_device.reset();
}

void InstancingLayer::onUpdate(core::Timestep delta_time) {
    m_time += delta_time.getSeconds();
    if (m_instancing_pass) {
        m_instancing_pass->setTime(m_time);
    }
}

void InstancingLayer::onRender() {
    if (!m_render_device || !m_instancing_pass) {
        return;
    }

    renderer::RenderPass* pass = m_instancing_pass.get();
    const std::array passes = { pass };
    if (!m_render_device->renderFrame(
                std::span<renderer::RenderPass* const>{ passes.data(), passes.size() })) {
        return;
    }

    if (m_smoke && m_smoke_frames_remaining == 5) {
        core::Application::get().getWindow().resize(960, 540);
    }
    if (m_smoke && --m_smoke_frames_remaining == 0) {
        core::Application::get().getLogChannel().info(
                "3D instancing draw/depth/resize smoke test passed");
        core::Application::get().close();
    }
}

} // namespace arti::instancing
