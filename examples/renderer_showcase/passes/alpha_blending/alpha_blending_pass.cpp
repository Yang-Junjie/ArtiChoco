#include "passes/alpha_blending/alpha_blending_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"

#include <utility>

namespace arti::renderer_showcase {

AlphaBlendingPass::AlphaBlendingPass(renderer::VertexBuffer vertex_buffer,
                                     renderer::IndexBuffer index_buffer,
                                     std::filesystem::path shader_path)
    : IndexedGraphicsPass(std::move(vertex_buffer),
                          std::move(index_buffer),
                          std::move(shader_path),
                          {0.018f, 0.022f, 0.035f, 1.0f})
{}

AlphaBlendingPass::~AlphaBlendingPass() = default;

void AlphaBlendingPass::configurePipeline(
    renderer::vulkan::VulkanGraphicsPipelineCreateInfo& info) const
{
    vk::PipelineColorBlendAttachmentState blend;
    blend.setBlendEnable(true)
        .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
        .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
        .setColorBlendOp(vk::BlendOp::eAdd)
        .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
        .setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
        .setAlphaBlendOp(vk::BlendOp::eAdd)
        .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    info.color_blend_attachments = {blend};
}

} // namespace arti::renderer_showcase
