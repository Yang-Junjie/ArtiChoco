#pragma once
#include "layer.h"

#include <cstdint>

#include <memory>

namespace arti::renderer {
class Renderer;
}

namespace arti::test_app {
class ThrowOncePass;
class TextureComputePass;
class MrtMeshPass;
class MrtCompositePass;

class TestAppLayer final : public core::Layer {
public:
    explicit TestAppLayer(bool smoke_vulkan = false, bool enable_renderer = false, bool smoke_render = false);
    ~TestAppLayer() override;

    void onAttach() override;
    void onDetach() override;
    void onUpdate(core::Timestep delta_time) override;
    void onRender() override;

private:
    struct Mesh;
    struct Material;

    bool m_smoke_vulkan{false};
    bool m_enable_renderer{false};
    bool m_smoke_render{false};
    bool m_frame_recovery_awaiting_success{false};
    uint32_t m_render_frames_remaining{0};
    float m_elapsed_time{0.0f};
    std::unique_ptr<renderer::Renderer> m_renderer;
    std::unique_ptr<Mesh> m_mesh;
    std::unique_ptr<Material> m_material;
    std::unique_ptr<ThrowOncePass> m_throw_once_pass;
    std::unique_ptr<TextureComputePass> m_texture_compute_pass;
    std::unique_ptr<MrtMeshPass> m_mrt_mesh_pass;
    std::unique_ptr<MrtCompositePass> m_mrt_composite_pass;
};
} // namespace arti::test_app
