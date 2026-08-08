#pragma once
#include "passes/common/sampled_image_source.h"
#include "passes/common/showcase_pass.h"

#include <filesystem>
#include <memory>

namespace arti::renderer {
class IndexBuffer;
class VertexBuffer;
} // namespace arti::renderer

namespace arti::renderer_showcase {

class OffscreenPass final : public ShowcasePass, public SampledImageSource {
public:
    OffscreenPass(renderer::VertexBuffer vertex_buffer,
                  renderer::IndexBuffer index_buffer,
                  std::filesystem::path shader_path);
    ~OffscreenPass() override;

    void setElapsedTime(float elapsed_time) noexcept override;
    void prepare(renderer::vulkan::VulkanPassPrepareContext& context) override;
    void record(renderer::vulkan::VulkanPassContext& context) override;
    const renderer::vulkan::VulkanImage& output() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::renderer_showcase
