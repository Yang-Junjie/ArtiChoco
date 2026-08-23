#pragma once

#include "layer.h"

#include <cstdint>
#include <memory>

namespace arti::renderer {
class RenderDevice;
}

namespace arti::instancing {

class InstancingPass;

class InstancingLayer final : public core::Layer {
public:
    explicit InstancingLayer(bool smoke = false);
    ~InstancingLayer() override;

    void onAttach() override;
    void onDetach() override;
    void onUpdate(core::Timestep delta_time) override;
    void onRender() override;

private:
    bool m_smoke{ false };
    uint32_t m_smoke_frames_remaining{ 0 };
    float m_time{ 0.0f };
    std::unique_ptr<renderer::RenderDevice> m_render_device;
    std::unique_ptr<InstancingPass> m_instancing_pass;
};

} // namespace arti::instancing
