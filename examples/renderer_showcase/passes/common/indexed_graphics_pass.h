#pragma once
#include "passes/common/showcase_pass.h"

#include <array>
#include <filesystem>
#include <memory>

namespace arti::renderer {
class IndexBuffer;
class VertexBuffer;
namespace vulkan {
class VulkanBindingLayout;
class VulkanPipeline;
struct VulkanGraphicsPipelineCreateInfo;
} // namespace vulkan
} // namespace arti::renderer

namespace arti::renderer_showcase {

class IndexedGraphicsPass : public ShowcasePass {
public:
    ~IndexedGraphicsPass() override;

    IndexedGraphicsPass(const IndexedGraphicsPass&) = delete;
    IndexedGraphicsPass& operator=(const IndexedGraphicsPass&) = delete;

    void setElapsedTime(float elapsed_time) noexcept override;

    void prepare(renderer::vulkan::VulkanPassPrepareContext& context) final;
    void record(renderer::vulkan::VulkanPassContext& context) final;

protected:
    IndexedGraphicsPass(renderer::VertexBuffer vertex_buffer,
                        renderer::IndexBuffer index_buffer,
                        std::filesystem::path shader_path,
                        std::array<float, 4> clear_color,
                        bool depth_test_enabled = false);

    virtual void prepareResources(renderer::vulkan::VulkanPassPrepareContext& context);
    virtual void bindResources(renderer::vulkan::VulkanPassContext& context,
                               const renderer::vulkan::VulkanPipeline& pipeline);
    virtual void configurePipeline(renderer::vulkan::VulkanGraphicsPipelineCreateInfo& info) const;
    virtual uint32_t instanceCount() const noexcept;

    const renderer::vulkan::VulkanBindingLayout& bindingLayout() const;
    float elapsedTime() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::renderer_showcase
