#include "artichoco/renderer/renderer_log.h"
#include "vulkan_pipeline.h"
#include "vulkan_pipeline_layout.h"

#include <algorithm>
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

vk::PipelineColorBlendAttachmentState opaqueBlendAttachment() noexcept
{
    vk::PipelineColorBlendAttachmentState blend;
    blend.setBlendEnable(false).setColorWriteMask(
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
        vk::ColorComponentFlagBits::eA);
    return blend;
}

bool equalBlendState(const vk::PipelineColorBlendAttachmentState& lhs,
                     const vk::PipelineColorBlendAttachmentState& rhs) noexcept
{
    return lhs.blendEnable == rhs.blendEnable && lhs.srcColorBlendFactor == rhs.srcColorBlendFactor &&
        lhs.dstColorBlendFactor == rhs.dstColorBlendFactor && lhs.colorBlendOp == rhs.colorBlendOp &&
        lhs.srcAlphaBlendFactor == rhs.srcAlphaBlendFactor &&
        lhs.dstAlphaBlendFactor == rhs.dstAlphaBlendFactor && lhs.alphaBlendOp == rhs.alphaBlendOp &&
        lhs.colorWriteMask == rhs.colorWriteMask;
}

} // namespace

VulkanPipeline::VulkanPipeline(const VulkanDevice& device,
                               const VulkanShader& shader,
                               const VertexBufferLayout& vertex_layout,
                               const VulkanBindingLayout& binding_layout,
                               const VulkanGraphicsPipelineCreateInfo& info)
    : m_vertex_layout(vertex_layout),
      m_color_formats(info.color_formats),
      m_depth_format(info.depth_format)
{
    if (info.color_formats.empty() && info.depth_format == vk::Format::eUndefined) {
        throw std::invalid_argument("A Vulkan graphics pipeline requires at least one attachment format.");
    }
    if (std::ranges::any_of(info.color_formats, [](vk::Format format) {
            return format == vk::Format::eUndefined;
        })) {
        throw std::invalid_argument("Vulkan graphics pipeline color formats must be defined.");
    }
    if (!info.color_blend_attachments.empty() &&
        info.color_blend_attachments.size() != info.color_formats.size()) {
        throw std::invalid_argument("Every Vulkan color attachment requires one matching blend state.");
    }
    if (info.depth_format == vk::Format::eUndefined && (info.depth_test_enable || info.depth_write_enable)) {
        throw std::invalid_argument("A Vulkan graphics pipeline cannot use depth without a depth format.");
    }
    if (info.depth_write_enable && !info.depth_test_enable) {
        throw std::invalid_argument("Vulkan depth writes require depth testing to be enabled.");
    }

    m_layout = createPipelineLayout(device, binding_layout);

    const auto shader_stages = shader.stages();
    vk::VertexInputBindingDescription binding{};
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
    if (vertex_layout.stride == 0) {
        if (!attributes.empty()) {
            throw std::invalid_argument("A zero-stride Vulkan vertex layout cannot contain attributes.");
        }
    } else {
        binding.setBinding(0).setStride(vertex_layout.stride).setInputRate(vk::VertexInputRate::eVertex);
        vertex_input.setVertexBindingDescriptions(binding).setVertexAttributeDescriptions(attributes);
    }
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
    depth_stencil.setDepthTestEnable(info.depth_test_enable)
        .setDepthWriteEnable(info.depth_write_enable)
        .setDepthCompareOp(info.depth_compare_op)
        .setDepthBoundsTestEnable(false)
        .setStencilTestEnable(false);

    std::vector<vk::PipelineColorBlendAttachmentState> blend_attachments = info.color_blend_attachments;
    if (blend_attachments.empty()) {
        blend_attachments.resize(info.color_formats.size(), opaqueBlendAttachment());
    }
    if (!device.independentBlendEnabled() && blend_attachments.size() > 1 &&
        std::ranges::any_of(blend_attachments.begin() + 1,
                            blend_attachments.end(),
                            [&blend_attachments](const auto& blend) {
                                return !equalBlendState(blend_attachments.front(), blend);
                            })) {
        throw std::invalid_argument("The Vulkan device does not support independent color-attachment blending.");
    }
    vk::PipelineColorBlendStateCreateInfo color_blend{};
    color_blend.setLogicOpEnable(false).setAttachments(blend_attachments);

    constexpr std::array dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    vk::PipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.setDynamicStates(dynamic_states);

    vk::PipelineRenderingCreateInfo rendering_info{};
    rendering_info.setColorAttachmentFormats(info.color_formats).setDepthAttachmentFormat(info.depth_format);

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
    getLogChannel().info("Created Vulkan graphics pipeline with {} color attachment(s) and depth {}",
                         info.color_formats.size(),
                         vk::to_string(info.depth_format));
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

std::span<const vk::Format> VulkanPipeline::colorFormats() const noexcept
{
    return m_color_formats;
}

vk::Format VulkanPipeline::depthFormat() const noexcept
{
    return m_depth_format;
}

} // namespace arti::renderer::vulkan
