#include "vulkan_pipeline_cache.h"

#include <algorithm>
#include <functional>
#include <utility>

namespace arti::renderer::vulkan {
namespace {

size_t hashCombine(size_t seed, size_t value) noexcept
{
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

size_t hashVertexLayout(const VertexBufferLayout& layout) noexcept
{
    size_t seed = hashCombine(0, layout.stride);
    for (const VertexAttribute& attribute : layout.attributes) {
        seed = hashCombine(seed, attribute.location);
        seed = hashCombine(seed, static_cast<size_t>(attribute.type));
        seed = hashCombine(seed, attribute.offset);
    }
    return seed;
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

size_t hashBlendState(const vk::PipelineColorBlendAttachmentState& blend) noexcept
{
    size_t seed = 0;
    seed = hashCombine(seed, static_cast<size_t>(blend.blendEnable));
    seed = hashCombine(seed, static_cast<size_t>(blend.srcColorBlendFactor));
    seed = hashCombine(seed, static_cast<size_t>(blend.dstColorBlendFactor));
    seed = hashCombine(seed, static_cast<size_t>(blend.colorBlendOp));
    seed = hashCombine(seed, static_cast<size_t>(blend.srcAlphaBlendFactor));
    seed = hashCombine(seed, static_cast<size_t>(blend.dstAlphaBlendFactor));
    seed = hashCombine(seed, static_cast<size_t>(blend.alphaBlendOp));
    seed = hashCombine(seed, static_cast<uint32_t>(blend.colorWriteMask));
    return seed;
}

size_t hashGraphicsCreateInfo(const VulkanGraphicsPipelineCreateInfo& info) noexcept
{
    size_t seed = 0;
    for (const vk::Format format : info.color_formats) {
        seed = hashCombine(seed, static_cast<size_t>(format));
    }
    for (const vk::PipelineColorBlendAttachmentState& blend : info.color_blend_attachments) {
        seed = hashCombine(seed, hashBlendState(blend));
    }
    seed = hashCombine(seed, static_cast<size_t>(info.depth_format));
    seed = hashCombine(seed, static_cast<size_t>(info.depth_test_enable));
    seed = hashCombine(seed, static_cast<size_t>(info.depth_write_enable));
    seed = hashCombine(seed, static_cast<size_t>(info.depth_compare_op));
    return seed;
}

} // namespace

bool VulkanPipelineCache::GraphicsPipelineKey::operator==(const GraphicsPipelineKey& rhs) const noexcept
{
    return shader == rhs.shader && binding_layout == rhs.binding_layout &&
        vertex_layout == rhs.vertex_layout && info.color_formats == rhs.info.color_formats &&
        info.depth_format == rhs.info.depth_format && info.depth_test_enable == rhs.info.depth_test_enable &&
        info.depth_write_enable == rhs.info.depth_write_enable &&
        info.depth_compare_op == rhs.info.depth_compare_op &&
        std::ranges::equal(info.color_blend_attachments, rhs.info.color_blend_attachments, equalBlendState);
}

size_t VulkanPipelineCache::GraphicsPipelineKeyHash::operator()(const GraphicsPipelineKey& key) const noexcept
{
    size_t seed = hashCombine(std::hash<const VulkanShader*>{}(key.shader),
                              std::hash<const VulkanBindingLayout*>{}(key.binding_layout));
    seed = hashCombine(seed, hashVertexLayout(key.vertex_layout));
    seed = hashCombine(seed, hashGraphicsCreateInfo(key.info));
    return seed;
}

size_t VulkanPipelineCache::ComputePipelineKeyHash::operator()(const ComputePipelineKey& key) const noexcept
{
    return hashCombine(std::hash<const VulkanComputeShader*>{}(key.shader),
                       std::hash<const VulkanBindingLayout*>{}(key.binding_layout));
}

VulkanPipelineCache::VulkanPipelineCache(const VulkanDevice& device)
    : m_device(device)
{}

const VulkanPipeline& VulkanPipelineCache::getGraphics(const VulkanShader& shader,
                                                       const VulkanBindingLayout& binding_layout,
                                                       const VertexBufferLayout& vertex_layout,
                                                       const VulkanGraphicsPipelineCreateInfo& info)
{
    GraphicsPipelineKey key{&shader, &binding_layout, vertex_layout, info};
    const auto found = m_graphics.find(key);
    if (found != m_graphics.end()) {
        return *found->second;
    }

    auto pipeline = std::make_unique<VulkanPipeline>(m_device, shader, vertex_layout, binding_layout, info);
    const VulkanPipeline& created = *pipeline;
    m_graphics.emplace(std::move(key), std::move(pipeline));
    return created;
}

const VulkanComputePipeline& VulkanPipelineCache::getCompute(const VulkanComputeShader& shader,
                                                             const VulkanBindingLayout& binding_layout)
{
    const ComputePipelineKey key{&shader, &binding_layout};
    const auto found = m_compute.find(key);
    if (found != m_compute.end()) {
        return *found->second;
    }

    auto pipeline = std::make_unique<VulkanComputePipeline>(m_device, shader, binding_layout);
    const VulkanComputePipeline& created = *pipeline;
    m_compute.emplace(std::move(key), std::move(pipeline));
    return created;
}

void VulkanPipelineCache::clear() noexcept
{
    m_graphics.clear();
    m_compute.clear();
}

} // namespace arti::renderer::vulkan
