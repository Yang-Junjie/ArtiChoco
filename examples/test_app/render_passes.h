#pragma once
#include "artichoco/renderer/vulkan/vulkan_pass.h"

#include <array>
#include <filesystem>
#include <memory>

namespace arti::renderer {
class IndexBuffer;
class Texture2D;
class VertexBuffer;

namespace vulkan {
class VulkanImage;
} // namespace vulkan
} // namespace arti::renderer

namespace arti::test_app {

class ThrowOncePass final : public renderer::vulkan::VulkanPass {
public:
    void record(renderer::vulkan::VulkanPassContext& context) override;
    bool didThrow() const noexcept;

private:
    bool m_did_throw{false};
};

class TextureComputePass final : public renderer::vulkan::VulkanPass {
public:
    TextureComputePass(const renderer::Texture2D& source, const std::filesystem::path& shader_path);
    ~TextureComputePass() override;

    void setSource(const renderer::Texture2D& source);
    void setTime(float time) noexcept;
    const renderer::vulkan::VulkanImage& output() const;
    void prepare(renderer::vulkan::VulkanPassPrepareContext& context) override;
    void record(renderer::vulkan::VulkanPassContext& context) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

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

class MrtCompositePass final : public renderer::vulkan::VulkanPass {
public:
    MrtCompositePass(MrtMeshPass& source, const std::filesystem::path& shader_path);
    ~MrtCompositePass() override;

    void prepare(renderer::vulkan::VulkanPassPrepareContext& context) override;
    void record(renderer::vulkan::VulkanPassContext& context) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::test_app
