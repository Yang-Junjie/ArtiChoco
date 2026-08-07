#pragma once
#include "artichoco/scene/system.h"

#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace arti::core {
class Window;
} // namespace arti::core

namespace arti::renderer {
class RenderDevice;
namespace vulkan {
class VulkanPass;
} // namespace vulkan
} // namespace arti::renderer

namespace arti::test_app {
class TextureComputePass;
class MrtMeshPass;
class MrtCompositePass;

class RenderSystem final : public scene::SceneSystem {
public:
    RenderSystem(renderer::RenderDevice& render_device,
                 core::Window& window,
                 std::filesystem::path compute_shader_path,
                 std::filesystem::path mesh_shader_path,
                 std::filesystem::path composite_shader_path);
    ~RenderSystem() override;

    void onUpdate(scene::Scene& scene, const scene::UpdateContext& context) override;

    void prependPass(renderer::vulkan::VulkanPass* pass);
    void removePass(renderer::vulkan::VulkanPass* pass) noexcept;
    std::span<renderer::vulkan::VulkanPass* const> passes() const noexcept;
private:
    renderer::RenderDevice& m_render_device;
    core::Window& m_window;
    std::filesystem::path m_compute_shader_path;
    std::filesystem::path m_mesh_shader_path;
    std::filesystem::path m_composite_shader_path;

    std::unique_ptr<TextureComputePass> m_texture_compute_pass;
    std::unique_ptr<MrtMeshPass> m_mrt_mesh_pass;
    std::unique_ptr<MrtCompositePass> m_mrt_composite_pass;
    std::vector<renderer::vulkan::VulkanPass*> m_passes;
    bool m_passes_initialized{false};
    float m_elapsed_time{0.0f};
};

} // namespace arti::test_app
