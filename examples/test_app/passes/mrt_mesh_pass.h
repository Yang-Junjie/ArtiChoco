#pragma once
#include "artichoco/renderer/vulkan/vulkan_pass.h"
#include "texture_compute_pass.h"

#include <array>
#include <filesystem>
#include <memory>

namespace arti::renderer {
class IndexBuffer;
class VertexBuffer;
namespace vulkan {
class VulkanImage;
} // namespace vulkan
} // namespace arti::renderer

namespace arti::test_app {

class MrtMeshPass final : public renderer::vulkan::VulkanPass {
public:
    MrtMeshPass(TextureComputePass& texture_source, const std::filesystem::path& shader_path);
    ~MrtMeshPass() override;

    void setGeometry(const renderer::VertexBuffer& vertex_buffer, const renderer::IndexBuffer& index_buffer) noexcept;
    void setTransform(const std::array<float, 16>& transform) noexcept;
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
