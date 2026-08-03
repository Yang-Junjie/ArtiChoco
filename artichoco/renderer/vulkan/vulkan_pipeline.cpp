#include "vulkan_pipeline.h"

#include "artichoco/renderer/renderer_log.h"

#include <array>

namespace arti::renderer::vulkan {

VulkanPipeline::VulkanPipeline(const VulkanDevice& device, const VulkanShader& shader, vk::Format color_format)
    : m_color_format(color_format)
{
    m_layout = vk::raii::PipelineLayout{device.device(), vk::PipelineLayoutCreateInfo{}};

    const auto shader_stages = shader.stages();
    vk::PipelineVertexInputStateCreateInfo vertex_input{};
    vk::PipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.setTopology(vk::PrimitiveTopology::eTriangleList).setPrimitiveRestartEnable(false);

    vk::PipelineViewportStateCreateInfo viewport_state{};
    viewport_state.setViewportCount(1).setScissorCount(1);

    vk::PipelineRasterizationStateCreateInfo rasterization{};
    rasterization.setDepthClampEnable(false)
        .setRasterizerDiscardEnable(false)
        .setPolygonMode(vk::PolygonMode::eFill)
        .setCullMode(vk::CullModeFlagBits::eNone)
        .setFrontFace(vk::FrontFace::eCounterClockwise)
        .setDepthBiasEnable(false)
        .setLineWidth(1.0f);

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.setRasterizationSamples(vk::SampleCountFlagBits::e1).setSampleShadingEnable(false);

    vk::PipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.setBlendEnable(false)
        .setColorWriteMask(
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA);
    vk::PipelineColorBlendStateCreateInfo color_blend{};
    color_blend.setLogicOpEnable(false).setAttachments(blend_attachment);

    constexpr std::array dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    vk::PipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.setDynamicStates(dynamic_states);

    const std::array color_formats = {color_format};
    vk::PipelineRenderingCreateInfo rendering_info{};
    rendering_info.setColorAttachmentFormats(color_formats);

    vk::GraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.setPNext(&rendering_info)
        .setStages(shader_stages)
        .setPVertexInputState(&vertex_input)
        .setPInputAssemblyState(&input_assembly)
        .setPViewportState(&viewport_state)
        .setPRasterizationState(&rasterization)
        .setPMultisampleState(&multisampling)
        .setPColorBlendState(&color_blend)
        .setPDynamicState(&dynamic_state)
        .setLayout(*m_layout);

    m_pipeline = vk::raii::Pipeline{device.device(), nullptr, pipeline_info};
    getLogChannel().info("Created Vulkan graphics pipeline for {}", vk::to_string(color_format));
}

const vk::raii::Pipeline& VulkanPipeline::handle() const noexcept
{
    return m_pipeline;
}

const vk::raii::PipelineLayout& VulkanPipeline::layout() const noexcept
{
    return m_layout;
}

vk::Format VulkanPipeline::colorFormat() const noexcept
{
    return m_color_format;
}

} // namespace arti::renderer::vulkan
