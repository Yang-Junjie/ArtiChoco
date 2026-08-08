#pragma once
#include "passes/common/indexed_graphics_pass.h"

namespace arti::renderer {
class IndexBuffer;
class VertexBuffer;
} // namespace arti::renderer

namespace arti::renderer_showcase {

class AlphaBlendingPass final : public IndexedGraphicsPass {
public:
    AlphaBlendingPass(renderer::VertexBuffer vertex_buffer,
                      renderer::IndexBuffer index_buffer,
                      std::filesystem::path shader_path);
    ~AlphaBlendingPass() override;

private:
    void configurePipeline(renderer::vulkan::VulkanGraphicsPipelineCreateInfo& info) const override;
};

} // namespace arti::renderer_showcase
