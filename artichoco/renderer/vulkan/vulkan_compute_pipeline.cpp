#include "artichoco/renderer/renderer_log.h"
#include "vulkan_compute_pipeline.h"
#include "vulkan_pipeline_layout.h"

namespace arti::renderer::vulkan {

VulkanComputePipeline::VulkanComputePipeline(const VulkanDevice& device,
                                             const VulkanComputeShader& shader,
                                             const VulkanBindingLayout& binding_layout)
{
    m_layout = createPipelineLayout(device, binding_layout);
    vk::ComputePipelineCreateInfo create_info{};
    create_info.setStage(shader.stage()).setLayout(*m_layout);
    m_pipeline = vk::raii::Pipeline{device.device(), nullptr, create_info};
    getLogChannel().info("Created Vulkan compute pipeline");
}

const vk::raii::Pipeline& VulkanComputePipeline::handle() const noexcept
{
    return m_pipeline;
}

const vk::raii::PipelineLayout& VulkanComputePipeline::layout() const noexcept
{
    return m_layout;
}

} // namespace arti::renderer::vulkan
