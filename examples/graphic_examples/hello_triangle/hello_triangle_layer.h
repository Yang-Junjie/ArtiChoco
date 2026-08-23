#pragma once

#include "layer.h"

#include <cstdint>
#include <memory>

namespace arti::renderer {
class RenderDevice;
}

namespace arti::hello_triangle {

class TrianglePass;

class HelloTriangleLayer final : public core::Layer {
public:
    explicit HelloTriangleLayer(bool smoke = false);
    ~HelloTriangleLayer() override;

    void onAttach() override;
    void onDetach() override;
    void onRender() override;

private:
    bool m_smoke{false};
    uint32_t m_smoke_frames_remaining{0};
    std::unique_ptr<renderer::RenderDevice> m_render_device;
    std::unique_ptr<TrianglePass> m_triangle_pass;
};

} // namespace arti::hello_triangle
