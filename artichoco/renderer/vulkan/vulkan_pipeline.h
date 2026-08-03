#pragma once
#include "artichoco/renderer/vertex_buffer.h"
#include "vulkan_device.h"
#include "vulkan_shader.h"

#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

class VulkanPipeline {
public:
    VulkanPipeline(
        const VulkanDevice& device,
        const VulkanShader& shader,
        const VertexBufferLayout& vertex_layout,
        vk::DescriptorSetLayout texture_layout,
        vk::Format color_format,
        vk::Format depth_format);

    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    const vk::raii::Pipeline& handle() const noexcept;
    const vk::raii::PipelineLayout& layout() const noexcept;
    const VertexBufferLayout& vertexLayout() const noexcept;
    vk::Format colorFormat() const noexcept;
    vk::Format depthFormat() const noexcept;

private:
    vk::raii::PipelineLayout m_layout{nullptr};
    vk::raii::Pipeline m_pipeline{nullptr};
    VertexBufferLayout m_vertex_layout;
    vk::Format m_color_format{vk::Format::eUndefined};
    vk::Format m_depth_format{vk::Format::eUndefined};
};

} // namespace arti::renderer::vulkan
