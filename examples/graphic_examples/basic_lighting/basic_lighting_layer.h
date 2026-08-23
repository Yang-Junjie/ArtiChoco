#pragma once

#include "layer.h"

#include <cstdint>
#include <memory>

namespace arti::renderer {
class RenderDevice;
}

namespace arti::basic_lighting {

class LightingPass;
class PresentPass;

class BasicLightingLayer final : public core::Layer {
public:
    explicit BasicLightingLayer(bool smoke = false);
    ~BasicLightingLayer() override;

    void onAttach() override;
    void onDetach() override;
    void onUpdate(core::Timestep delta_time) override;
    void onRender() override;

private:
    bool m_smoke{ false };
    uint32_t m_smoke_frames_remaining{ 0 };
    float m_rotation{ 0.0f };
    std::unique_ptr<renderer::RenderDevice> m_render_device;
    std::unique_ptr<LightingPass> m_lighting_pass;
    std::unique_ptr<PresentPass> m_present_pass;
};

} // namespace arti::basic_lighting
