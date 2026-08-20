#include "passes/common/image_display_pass.h"

#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_binding_layout.h"
#include "artichoco/renderer/vulkan/vulkan_binding_set.h"
#include "artichoco/renderer/vulkan/vulkan_command_recorder.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_image.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline_cache.h"
#include "artichoco/renderer/vulkan/vulkan_resource_state.h"
#include "artichoco/renderer/vulkan/vulkan_sampler.h"
#include "artichoco/renderer/vulkan/vulkan_shader.h"
#include "passes/common/sampled_image_source.h"

#include <array>
#include <utility>
#include <vector>

namespace arti::renderer_showcase {

struct ImageDisplayPass::Impl {
    Impl(SampledImageSource& source, std::filesystem::path shader_path)
        : source(&source),
          shader_path(std::move(shader_path))
    {}

    SampledImageSource* source;
    std::filesystem::path shader_path;
    std::unique_ptr<renderer::vulkan::VulkanShader> shader;
    std::unique_ptr<renderer::vulkan::VulkanBindingLayout> binding_layout;
    std::unique_ptr<renderer::vulkan::VulkanSampler> sampler;
    std::vector<renderer::vulkan::VulkanBindingSet> binding_sets;
};

ImageDisplayPass::ImageDisplayPass(SampledImageSource& source, std::filesystem::path shader_path)
    : m_impl(std::make_unique<Impl>(source, std::move(shader_path)))
{}

ImageDisplayPass::~ImageDisplayPass() = default;

void ImageDisplayPass::prepare(renderer::vulkan::VulkanPassPrepareContext& context)
{
    if (m_impl->shader) {
        return;
    }

    auto program = renderer::SlangCompiler::compileGraphics({m_impl->shader_path});
    m_impl->binding_layout =
        std::make_unique<renderer::vulkan::VulkanBindingLayout>(context.device(), program.reflection);
    m_impl->shader =
        std::make_unique<renderer::vulkan::VulkanShader>(context.device(), std::move(program));
    m_impl->sampler = std::make_unique<renderer::vulkan::VulkanSampler>(context.device());
    m_impl->binding_sets.reserve(context.frameSlotCount());
    for (size_t index = 0; index < context.frameSlotCount(); ++index) {
        m_impl->binding_sets.emplace_back(context.device(), context.descriptorAllocator(), *m_impl->binding_layout);
    }
}

void ImageDisplayPass::record(renderer::vulkan::VulkanPassContext& context)
{
    auto& frame = context.frame();
    renderer::vulkan::VulkanGraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.color_formats = {frame.colorFormat()};
    const renderer::VertexBufferLayout empty_layout;
    const auto& pipeline = context.pipelineCache().getGraphics(
        *m_impl->shader, *m_impl->binding_layout, empty_layout, pipeline_info);

    auto& bindings = m_impl->binding_sets.at(frame.frameSlotIndex());
    bindings.writeSampledImage("source_image", *m_impl->source->output().imageView());
    bindings.writeSampler("source_sampler", *m_impl->sampler->handle());

    context.commands().imageBarrier(renderer::vulkan::makeImageBarrier(frame.colorImage(),
        vk::ImageAspectFlagBits::eColor, renderer::vulkan::undefinedImageState(),
        renderer::vulkan::colorAttachmentWriteState()));

    vk::RenderingAttachmentInfo color_attachment;
    color_attachment.setImageView(frame.colorImageView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearValue{vk::ClearColorValue{std::array{0.012f, 0.016f, 0.025f, 1.0f}}});
    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea(vk::Rect2D{{0, 0}, frame.extent()})
        .setLayerCount(1)
        .setColorAttachments(color_attachment);

    context.commands().beginRendering(rendering_info);
    context.commands().setViewportAndScissor(frame.extent());
    context.commands().bindPipeline(pipeline);
    context.commands().bindBindingSet(pipeline, bindings);
    context.commands().draw(3);
    context.commands().endRendering();

    context.commands().imageBarrier(renderer::vulkan::makeImageBarrier(frame.colorImage(),
        vk::ImageAspectFlagBits::eColor, renderer::vulkan::colorAttachmentWriteState(),
        renderer::vulkan::presentState()));
}

} // namespace arti::renderer_showcase
