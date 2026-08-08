#pragma once
#include "passes/common/indexed_graphics_pass.h"

namespace arti::renderer {
class IndexBuffer;
class VertexBuffer;
} // namespace arti::renderer

namespace arti::renderer_showcase {

class PushConstantsPass final : public IndexedGraphicsPass {
public:
    PushConstantsPass(renderer::VertexBuffer vertex_buffer,
                      renderer::IndexBuffer index_buffer,
                      std::filesystem::path shader_path);
    ~PushConstantsPass() override;

private:
    void prepareResources(renderer::vulkan::VulkanPassPrepareContext& context) override;
    void bindResources(renderer::vulkan::VulkanPassContext& context,
                       const renderer::vulkan::VulkanPipeline& pipeline) override;
};

} // namespace arti::renderer_showcase
