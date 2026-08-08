#pragma once
#include "passes/common/indexed_graphics_pass.h"

namespace arti::renderer {
class IndexBuffer;
class TextureCube;
class VertexBuffer;
} // namespace arti::renderer

namespace arti::renderer_showcase {

class CubemapPass final : public IndexedGraphicsPass {
public:
    CubemapPass(renderer::VertexBuffer vertex_buffer, renderer::IndexBuffer index_buffer,
            renderer::TextureCube texture, std::filesystem::path shader_path);
    ~CubemapPass() override;

private:
    void prepareResources(renderer::vulkan::VulkanPassPrepareContext& context) override;
    void bindResources(renderer::vulkan::VulkanPassContext& context,
            const renderer::vulkan::VulkanPipeline& pipeline) override;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::renderer_showcase
