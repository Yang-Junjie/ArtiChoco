#pragma once
#include "artichoco/scene/entity.h"
#include "layer.h"
#include "scene_components.h"

#include <cstdint>

#include <memory>

namespace arti::scene {
class Scene;
} // namespace arti::scene

namespace arti::renderer {
class RenderDevice;
} // namespace arti::renderer

namespace arti::test_app {
class ThrowOncePass;

class TestAppLayer final : public core::Layer {
public:
    explicit TestAppLayer(bool smoke_vulkan = false, bool enable_renderer = false, bool smoke_render = false);
    ~TestAppLayer() override;

    void onAttach() override;
    void onDetach() override;
    void onUpdate(core::Timestep delta_time) override;
    void onRender() override;

private:
    bool m_smoke_vulkan{false};
    bool m_enable_renderer{false};
    bool m_smoke_render{false};
    bool m_frame_recovery_awaiting_success{false};
    uint32_t m_render_frames_remaining{0};
    uint32_t m_frame_index{0};
    core::Timestep m_delta_time{};
    std::unique_ptr<renderer::RenderDevice> m_render_device;
    std::unique_ptr<scene::Scene> m_scene;
    scene::Entity m_cube_entity;
    std::unique_ptr<ThrowOncePass> m_throw_once_pass;
};
} // namespace arti::test_app
