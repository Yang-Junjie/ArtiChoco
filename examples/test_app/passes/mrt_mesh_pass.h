#pragma once
#include "artichoco/renderer/render_frame_data.h"
#include "artichoco/renderer/vulkan/vulkan_pass.h"
#include "artichoco/renderer/vulkan/vulkan_sampler.h"
#include "texture_compute_pass.h"

#include <array>
#include <filesystem>
#include <memory>

namespace arti::renderer {
namespace vulkan {
class VulkanImage;
} // namespace vulkan
} // namespace arti::renderer

namespace arti::test_app {

class MrtMeshPass final : public renderer::vulkan::VulkanPass {
public:
    MrtMeshPass(TextureComputePass& texture_source, const std::filesystem::path& shader_path);
    ~MrtMeshPass() override;

    void setClearColor(const std::array<float, 4>& color) noexcept;
    const renderer::vulkan::VulkanImage& colorOutput() const;
    const renderer::vulkan::VulkanImage& auxiliaryOutput() const;
    void prepare(renderer::vulkan::VulkanPassPrepareContext& context) override;
    void record(renderer::vulkan::VulkanPassContext& context) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::test_app
