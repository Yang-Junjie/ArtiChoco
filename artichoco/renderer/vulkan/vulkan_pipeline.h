#pragma once
#include "vulkan_device.h"
#include "vulkan_shader.h"

#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

class VulkanPipeline {
public:
    VulkanPipeline(const VulkanDevice& device, const VulkanShader& shader, vk::Format color_format);

    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    const vk::raii::Pipeline& handle() const noexcept;
    const vk::raii::PipelineLayout& layout() const noexcept;
    vk::Format colorFormat() const noexcept;

private:
    vk::raii::PipelineLayout m_layout{nullptr};
    vk::raii::Pipeline m_pipeline{nullptr};
    vk::Format m_color_format{vk::Format::eUndefined};
};

} // namespace arti::renderer::vulkan
