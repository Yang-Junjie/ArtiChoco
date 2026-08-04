#pragma once
#include "artichoco/renderer/vertex_buffer.h"
#include "vulkan_binding_layout.h"
#include "vulkan_device.h"
#include "vulkan_shader.h"

#include <span>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

struct VulkanGraphicsPipelineCreateInfo {
    std::vector<vk::Format> color_formats;
    std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachments;
    vk::Format depth_format{vk::Format::eUndefined};
    bool depth_test_enable{false};
    bool depth_write_enable{false};
    vk::CompareOp depth_compare_op{vk::CompareOp::eLess};
};

class VulkanPipeline {
public:
    VulkanPipeline(const VulkanDevice& device,
                   const VulkanShader& shader,
                   const VertexBufferLayout& vertex_layout,
                   const VulkanBindingLayout& binding_layout,
                   const VulkanGraphicsPipelineCreateInfo& info);

    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    const vk::raii::Pipeline& handle() const noexcept;
    const vk::raii::PipelineLayout& layout() const noexcept;
    const VertexBufferLayout& vertexLayout() const noexcept;
    std::span<const vk::Format> colorFormats() const noexcept;
    vk::Format depthFormat() const noexcept;

private:
    vk::raii::PipelineLayout m_layout{nullptr};
    vk::raii::Pipeline m_pipeline{nullptr};
    VertexBufferLayout m_vertex_layout;
    std::vector<vk::Format> m_color_formats;
    vk::Format m_depth_format{vk::Format::eUndefined};
};

} // namespace arti::renderer::vulkan
