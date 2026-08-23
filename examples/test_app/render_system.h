#pragma once
#include "artichoco/renderer/render_frame_queue.h"
#include "artichoco/scene/system.h"
#include "frame_data.h"

#include <atomic>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

namespace arti::core {
class Window;
} // namespace arti::core

namespace arti::renderer {
class RenderDevice;
class RenderPass;
} // namespace arti::renderer

namespace arti::test_app {
class TextureComputePass;
class MrtMeshPass;
class MrtCompositePass;

class RenderSystem final : public scene::SceneSystem {
public:
    RenderSystem(renderer::RenderDevice& render_device,
                 core::Window& window,
                 size_t render_slots,
                 std::filesystem::path compute_shader_path,
                 std::filesystem::path mesh_shader_path,
                 std::filesystem::path composite_shader_path);
    ~RenderSystem() override;

    void onAttach(scene::Scene& scene) override;
    void onDetach(scene::Scene& scene) override;
    void onUpdate(scene::Scene& scene, const scene::UpdateContext& context) override;

    void prependPass(renderer::RenderPass* pass);
    void removePass(renderer::RenderPass* pass) noexcept;

    void waitForFrameComplete();
    std::exception_ptr consumeRenderError();
    size_t lastDrawCount() const noexcept;

private:
    void ensurePasses(scene::Scene& scene);
    void renderThreadLoop();
    std::vector<renderer::RenderPass*> snapshotPasses();

    renderer::RenderDevice& m_render_device;
    core::Window& m_window;
    std::filesystem::path m_compute_shader_path;
    std::filesystem::path m_mesh_shader_path;
    std::filesystem::path m_composite_shader_path;

    renderer::RenderFrameQueue<RenderFrameData> m_frame_queue;
    std::atomic<bool> m_shutdown_requested{false};
    std::mutex m_pass_mutex;
    std::mutex m_error_mutex;
    std::exception_ptr m_last_render_error;
    bool m_render_thread_started{false};

    std::unique_ptr<TextureComputePass> m_texture_compute_pass;
    std::unique_ptr<MrtMeshPass> m_mrt_mesh_pass;
    std::unique_ptr<MrtCompositePass> m_mrt_composite_pass;
    std::vector<renderer::RenderPass*> m_passes;
    bool m_passes_initialized{false};
    float m_elapsed_time{0.0f};
    std::atomic<size_t> m_last_draw_count{0};
};

} // namespace arti::test_app
