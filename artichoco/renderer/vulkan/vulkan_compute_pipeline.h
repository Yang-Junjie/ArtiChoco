#pragma once
#include "vulkan_binding_layout.h"
#include "vulkan_compute_shader.h"

#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

class VulkanComputePipeline {
public:
    VulkanComputePipeline(const VulkanDevice& device,
                          const VulkanComputeShader& shader,
                          const VulkanBindingLayout& binding_layout);

    VulkanComputePipeline(const VulkanComputePipeline&) = delete;
    VulkanComputePipeline& operator=(const VulkanComputePipeline&) = delete;

    const vk::raii::Pipeline& handle() const noexcept;
    const vk::raii::PipelineLayout& layout() const noexcept;

private:
    vk::raii::PipelineLayout m_layout{nullptr};
    vk::raii::Pipeline m_pipeline{nullptr};
};

} // namespace arti::renderer::vulkan
