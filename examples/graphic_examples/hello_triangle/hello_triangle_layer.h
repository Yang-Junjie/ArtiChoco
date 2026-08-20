#pragma once
#include "layer.h"

#include <memory>

namespace arti::renderer {
class RenderDevice;
} // namespace arti::renderer

namespace arti::hello_triangle {
class TriangleRenderer;

class HelloTriangleLayer final : public core::Layer {
public:
    HelloTriangleLayer();
    ~HelloTriangleLayer() override;

    void onAttach() override;
    void onDetach() override;
    void onRender() override;

private:
    std::unique_ptr<renderer::RenderDevice> m_render_device;
    std::unique_ptr<TriangleRenderer> m_renderer;
};

} // namespace arti::hello_triangle
