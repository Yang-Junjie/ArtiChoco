#include "artichoco/renderer/renderer_log.h"
#include "vulkan_pipeline.h"
#include "vulkan_pipeline_layout.h"

#include <array>
#include <stdexcept>
#include <vector>

namespace arti::renderer::vulkan {
namespace {

vk::Format toVulkanFormat(VertexAttributeType type)
{
    switch (type) {
        case VertexAttributeType::Float2:
            return vk::Format::eR32G32Sfloat;
        case VertexAttributeType::Float3:
            return vk::Format::eR32G32B32Sfloat;
        case VertexAttributeType::Float4:
            return vk::Format::eR32G32B32A32Sfloat;
    }
    throw std::invalid_argument("Unsupported vertex attribute type.");
}

} // namespace

VulkanPipeline::VulkanPipeline(const VulkanDevice& device,
                               const VulkanShader& shader,
                               const VertexBufferLayout& vertex_layout,
                               const VulkanBindingLayout& binding_layout,
                               vk::Format color_format,
                               vk::Format depth_format)
    : m_vertex_layout(vertex_layout),
      m_color_format(color_format),
      m_depth_format(depth_format)
{
    m_layout = createPipelineLayout(device, binding_layout);

    const auto shader_stages = shader.stages();
    vk::VertexInputBindingDescription binding{};
    binding.setBinding(0).setStride(vertex_layout.stride).setInputRate(vk::VertexInputRate::eVertex);
    std::vector<vk::VertexInputAttributeDescription> attributes;
    attributes.reserve(vertex_layout.attributes.size());
    for (const auto& attribute : vertex_layout.attributes) {
        attributes.push_back(vk::VertexInputAttributeDescription{
            attribute.location,
            0,
            toVulkanFormat(attribute.type),
            attribute.offset,
        });
    }
    vk::PipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.setVertexBindingDescriptions(binding).setVertexAttributeDescriptions(attributes);
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

    vk::PipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.setDepthTestEnable(true)
        .setDepthWriteEnable(true)
        .setDepthCompareOp(vk::CompareOp::eLess)
        .setDepthBoundsTestEnable(false)
        .setStencilTestEnable(false);

    vk::PipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.setBlendEnable(false).setColorWriteMask(
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
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
    rendering_info.setColorAttachmentFormats(color_formats).setDepthAttachmentFormat(depth_format);

    vk::GraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.setPNext(&rendering_info)
        .setStages(shader_stages)
        .setPVertexInputState(&vertex_input)
        .setPInputAssemblyState(&input_assembly)
        .setPViewportState(&viewport_state)
        .setPRasterizationState(&rasterization)
        .setPMultisampleState(&multisampling)
        .setPDepthStencilState(&depth_stencil)
        .setPColorBlendState(&color_blend)
        .setPDynamicState(&dynamic_state)
        .setLayout(*m_layout);

    m_pipeline = vk::raii::Pipeline{device.device(), nullptr, pipeline_info};
    getLogChannel().info("Created Vulkan graphics pipeline for color {} and depth {}",
                         vk::to_string(color_format),
                         vk::to_string(depth_format));
}

const vk::raii::Pipeline& VulkanPipeline::handle() const noexcept
{
    return m_pipeline;
}

const vk::raii::PipelineLayout& VulkanPipeline::layout() const noexcept
{
    return m_layout;
}

const VertexBufferLayout& VulkanPipeline::vertexLayout() const noexcept
{
    return m_vertex_layout;
}

vk::Format VulkanPipeline::colorFormat() const noexcept
{
    return m_color_format;
}

vk::Format VulkanPipeline::depthFormat() const noexcept
{
    return m_depth_format;
}

} // namespace arti::renderer::vulkan
