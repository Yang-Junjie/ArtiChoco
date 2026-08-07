#pragma once
#include "artichoco/renderer/render_frame_data.h"
#include "artichoco/renderer/vulkan/vulkan_pass.h"
#include "artichoco/renderer/vulkan/vulkan_sampler.h"

#include <filesystem>
#include <memory>

namespace arti::renderer {
class Texture2D;
namespace vulkan {
class VulkanImage;
} // namespace vulkan
} // namespace arti::renderer

namespace arti::test_app {

class TextureComputePass final : public renderer::vulkan::VulkanPass {
public:
    TextureComputePass(std::shared_ptr<renderer::Texture2D> source, const std::filesystem::path& shader_path);
    ~TextureComputePass() override;

    const renderer::vulkan::VulkanImage& output() const;
    void prepare(renderer::vulkan::VulkanPassPrepareContext& context) override;
    void record(renderer::vulkan::VulkanPassContext& context) override;
    void applyFrameData(const renderer::RenderFrameData& frame_data) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::test_app
