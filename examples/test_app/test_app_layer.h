#pragma once
#include "artichoco/core/timestep_accumulator.h"
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
class RenderPass;
} // namespace arti::renderer

namespace arti::test_app {
class CubemapAttachmentPass;
class ThrowOncePass;
class TextureComputePass;

class TestAppLayer final : public core::Layer {
public:
    explicit TestAppLayer(bool enable_renderer = false,
            bool smoke_render = false, bool smoke_nvrhi = false);
    ~TestAppLayer() override;

    void onAttach() override;
    void onDetach() override;
    void onUpdate(core::Timestep delta_time) override;
    void onRender() override;

private:
    void verifyHierarchy();
    void verifySnapshot();
    void verifySerialization();
    void verifyTaskSystem();
    void verifyMultiEntity();
    void verifyProject();

private:
    bool m_enable_renderer{false};
    bool m_smoke_render{false};
    bool m_smoke_nvrhi{false};
    uint32_t m_render_frames_remaining{0};
    uint32_t m_frame_index{0};
    uint32_t m_fixed_frame_index{0};
    core::Timestep m_delta_time{};
    core::FixedTimestepAccumulator m_fixed_timestep;
    std::unique_ptr<renderer::RenderDevice> m_render_device;
    std::unique_ptr<renderer::RenderPass> m_nvrhi_clear_pass;
    std::unique_ptr<TextureComputePass> m_nvrhi_texture_compute_pass;
    std::unique_ptr<CubemapAttachmentPass> m_nvrhi_cubemap_attachment_pass;
    bool m_nvrhi_cubemap_verified{false};
    std::unique_ptr<scene::Scene> m_scene;
    scene::Entity m_cube_entity;
    scene::Entity m_cube_secondary_entity;
    glm::quat m_rotation_snapshot{1.0f, 0.0f, 0.0f, 0.0f};
    scene::Entity m_hierarchy_parent_entity;
    scene::Entity m_hierarchy_child_entity;
    std::unique_ptr<ThrowOncePass> m_throw_once_pass;
};
} // namespace arti::test_app
