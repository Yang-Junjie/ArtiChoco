#pragma once

#include "layer.h"

#include <cstdint>
#include <memory>

namespace arti::renderer {
class RenderDevice;
}

namespace arti::render_to_texture {

class DisplayPass;
class RenderTexturePass;

class RenderToTextureLayer final : public core::Layer {
public:
    explicit RenderToTextureLayer(bool smoke = false);
    ~RenderToTextureLayer() override;

    void onAttach() override;
    void onDetach() override;
    void onUpdate(core::Timestep delta_time) override;
    void onRender() override;

private:
    bool m_smoke{ false };
    uint32_t m_smoke_frames_remaining{ 0 };
    float m_time{ 0.0f };
    std::unique_ptr<renderer::RenderDevice> m_render_device;
    std::unique_ptr<RenderTexturePass> m_render_texture_pass;
    std::unique_ptr<DisplayPass> m_display_pass;
};

} // namespace arti::render_to_texture
