#pragma once
#include "layer.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace arti::renderer {
class RenderDevice;
} // namespace arti::renderer

namespace arti::renderer_showcase {
class RenderSystem;

class RendererShowcaseLayer final : public core::Layer {
public:
    explicit RendererShowcaseLayer(bool smoke_render = false);
    ~RendererShowcaseLayer() override;

    void onAttach() override;
    void onDetach() override;
    void onUpdate(core::Timestep delta_time) override;
    void onEvent(core::Event& event) override;
    void onRender() override;

private:
    void logActiveDemo() const;

    bool m_smoke_render{false};
    uint32_t m_frames_until_switch{0};
    size_t m_rendered_demo_count{0};
    float m_elapsed_time{0.0f};
    std::unique_ptr<renderer::RenderDevice> m_render_device;
    std::unique_ptr<RenderSystem> m_render_system;
};
} // namespace arti::renderer_showcase
