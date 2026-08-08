#pragma once
#include "passes/common/showcase_pass.h"

#include <filesystem>
#include <memory>

namespace arti::renderer {
class IndexBuffer;
class VertexBuffer;
} // namespace arti::renderer

namespace arti::renderer_showcase {

class RenderToCubemapPass final : public ShowcasePass {
public:
    RenderToCubemapPass(renderer::VertexBuffer vertex_buffer, renderer::IndexBuffer index_buffer,
            std::filesystem::path shader_path);
    ~RenderToCubemapPass() override;

    void setElapsedTime(float elapsed_time) noexcept override;
    void prepare(renderer::vulkan::VulkanPassPrepareContext& context) final;
    void record(renderer::vulkan::VulkanPassContext& context) final;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::renderer_showcase
