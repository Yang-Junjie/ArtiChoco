#pragma once
#include "layer.h"

#include <cstdint>
#include <memory>

namespace arti::renderer {
class Renderer;
}

namespace arti::test_app {
class TestAppLayer final : public core::Layer {
public:
    explicit TestAppLayer(bool smoke_vulkan = false, bool smoke_render = false);
    ~TestAppLayer() override;

    void onAttach() override;
    void onDetach() override;
    void onRender() override;

private:
    bool m_smoke_vulkan{false};
    bool m_smoke_render{false};
    uint32_t m_render_frames_remaining{0};
    std::unique_ptr<renderer::Renderer> m_renderer;
};
} // namespace arti::test_app
