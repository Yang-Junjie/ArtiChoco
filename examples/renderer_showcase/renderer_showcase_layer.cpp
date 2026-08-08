#include "application.h"
#include "artichoco/core/event/key_event.h"
#include "artichoco/platform/window/sdl_vulkan_surface_source.h"
#include "artichoco/renderer/render_device.h"
#include "render_system.h"
#include "renderer_showcase_layer.h"

#include <filesystem>
#include <utility>

namespace arti::renderer_showcase {
namespace {

constexpr uint32_t smokeFramesPerDemo = 2;

} // namespace

RendererShowcaseLayer::RendererShowcaseLayer(bool smoke_render)
    : Layer("RendererShowcaseLayer"),
      m_smoke_render(smoke_render),
      m_frames_until_switch(smoke_render ? smokeFramesPerDemo : 0u)
{}

RendererShowcaseLayer::~RendererShowcaseLayer() = default;

void RendererShowcaseLayer::onAttach()
{
    auto& application = core::Application::get();
    application.getLogChannel().info("Creating renderer showcase");

    auto surface_source = platform::createSDLVulkanSurfaceSource(application.getWindow());
    renderer::RenderDeviceCreateInfo device_info;
    device_info.application_name = "Renderer Showcase";
    m_render_device = std::make_unique<renderer::RenderDevice>(
        application.getWindow(), std::move(surface_source), device_info);

    m_render_system = std::make_unique<RenderSystem>(
        *m_render_device,
        std::filesystem::path{ARTI_RENDERER_SHOWCASE_SHADER_DIRECTORY},
        std::filesystem::path{ARTI_RENDERER_SHOWCASE_BROWNIE_TEXTURE_PATH});
    logActiveDemo();
}

void RendererShowcaseLayer::onDetach()
{
    if (m_render_device) {
        m_render_device->waitIdle();
    }
    m_render_system.reset();
    m_render_device.reset();
}

void RendererShowcaseLayer::onUpdate(core::Timestep delta_time)
{
    m_elapsed_time += delta_time.getSeconds();
}

void RendererShowcaseLayer::onEvent(core::Event& event)
{
    core::EventDispatcher dispatcher{event};
    dispatcher.dispatch<core::KeyPressedEvent>([this](const core::KeyPressedEvent& key_event) {
        if (key_event.getKeyCode() != core::KeyCode::Space || key_event.isRepeat() || !m_render_system) {
            return false;
        }
        m_render_system->nextDemo();
        logActiveDemo();
        return true;
    });
}

void RendererShowcaseLayer::onRender()
{
    if (!m_render_system || !m_render_system->renderFrame(m_elapsed_time)) {
        return;
    }

    if (!m_smoke_render || --m_frames_until_switch != 0) {
        return;
    }

    ++m_rendered_demo_count;
    if (m_rendered_demo_count == m_render_system->demoCount()) {
        m_render_device->waitIdle();
        core::Application::get().getLogChannel().info("Renderer showcase smoke render passed");
        core::Application::get().close();
        return;
    }

    m_render_system->nextDemo();
    m_frames_until_switch = smokeFramesPerDemo;
    logActiveDemo();
}

void RendererShowcaseLayer::logActiveDemo() const
{
    core::Application::get().getLogChannel().info(
        "Renderer demo: {}", m_render_system->activeDemoName());
}

} // namespace arti::renderer_showcase
