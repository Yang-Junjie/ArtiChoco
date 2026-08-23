#pragma once

#include "layer.h"

#include <cstdint>
#include <memory>

namespace arti::renderer {
class RenderDevice;
}

namespace arti::hello_cube {

class CubePass;

class HelloCubeLayer final : public core::Layer {
public:
    explicit HelloCubeLayer(bool smoke = false);
    ~HelloCubeLayer() override;

    void onAttach() override;
    void onDetach() override;
    void onUpdate(core::Timestep delta_time) override;
    void onRender() override;

private:
    bool m_smoke{ false };
    uint32_t m_smoke_frames_remaining{ 0 };
    float m_rotation{ 0.0f };
    std::unique_ptr<renderer::RenderDevice> m_render_device;
    std::unique_ptr<CubePass> m_cube_pass;
};

} // namespace arti::hello_cube
