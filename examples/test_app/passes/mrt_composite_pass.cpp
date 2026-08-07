#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_binding_layout.h"
#include "artichoco/renderer/vulkan/vulkan_binding_set.h"
#include "artichoco/renderer/vulkan/vulkan_device.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_image.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline_cache.h"
#include "artichoco/renderer/vulkan/vulkan_shader.h"
#include "mrt_composite_pass.h"
#include "render_pass_common.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

namespace arti::test_app {

struct MrtCompositePass::Impl {
    Impl(MrtMeshPass& source, std::filesystem::path shader_path)
        : source(&source),
          shader_path(std::move(shader_path))
    {}

    void initialize(renderer::vulkan::VulkanPassPrepareContext& context)
    {
        if (shader) {
            return;
        }

        auto program = renderer::SlangCompiler::compileGraphics({shader_path});
        binding_layout = std::make_unique<renderer::vulkan::VulkanBindingLayout>(context.device(), program.reflection);
        shader = std::make_unique<renderer::vulkan::VulkanShader>(context.device(), std::move(program));
        binding_sets.reserve(context.frameSlotCount());
        for (size_t index = 0; index < context.frameSlotCount(); ++index) {
            binding_sets.emplace_back(context.device(), context.descriptorAllocator(), *binding_layout);
        }
        output_sampler = renderer::vulkan::VulkanSampler{context.device()};
        device = &context.device();
    }

    MrtMeshPass* source{nullptr};
    std::filesystem::path shader_path;
    const renderer::vulkan::VulkanDevice* device{nullptr};
    std::unique_ptr<renderer::vulkan::VulkanShader> shader;
    std::unique_ptr<renderer::vulkan::VulkanBindingLayout> binding_layout;
    const renderer::vulkan::VulkanPipeline* pipeline{nullptr};
    renderer::vulkan::VulkanSampler output_sampler;
    std::vector<renderer::vulkan::VulkanBindingSet> binding_sets;
};

MrtCompositePass::MrtCompositePass(MrtMeshPass& source, const std::filesystem::path& shader_path)
    : m_impl(std::make_unique<Impl>(source, shader_path))
{}

MrtCompositePass::~MrtCompositePass() = default;

void MrtCompositePass::prepare(renderer::vulkan::VulkanPassPrepareContext& context)
{
    m_impl->initialize(context);
    (void) m_impl->source->colorOutput();
    (void) m_impl->source->auxiliaryOutput();
}

void MrtCompositePass::record(renderer::vulkan::VulkanPassContext& context)
{
    auto& frame = context.frame();
    const std::array color_formats = {frame.colorFormat()};
    const renderer::VertexBufferLayout empty_vertex_layout;
    renderer::vulkan::VulkanGraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.color_formats.assign(color_formats.begin(), color_formats.end());
    m_impl->pipeline =
        &context.pipelineCache().getGraphics(*m_impl->shader, *m_impl->binding_layout, empty_vertex_layout, pipeline_info);

    const auto& color_output = m_impl->source->colorOutput();
    const auto& auxiliary_output = m_impl->source->auxiliaryOutput();
    auto& bindings = m_impl->binding_sets.at(frame.frameSlotIndex());
    bindings.writeSampledImage("color_output", *color_output.imageView());
    bindings.writeSampledImage("auxiliary_output", *auxiliary_output.imageView());
    bindings.writeSampler("output_sampler", *m_impl->output_sampler.handle());

    const auto to_color_attachment = renderer::vulkan::makeImageBarrier(
        frame.colorImage(), colorSubresourceRange(), undefinedImageState(), colorAttachmentWriteState());
    auto& commands = context.commands();
    commands.imageBarrier(to_color_attachment);

    vk::RenderingAttachmentInfo color_attachment;
    color_attachment.setImageView(frame.colorImageView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearValue{vk::ClearColorValue{std::array{0.02f, 0.025f, 0.03f, 1.0f}}});
    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea(vk::Rect2D{{0, 0}, frame.extent()})
        .setLayerCount(1)
        .setColorAttachments(color_attachment);

    commands.beginRendering(rendering_info);
    commands.setViewportAndScissor(frame.extent());
    commands.bindPipeline(*m_impl->pipeline);
    commands.bindBindingSet(*m_impl->pipeline, bindings);
    commands.draw(3);
    commands.endRendering();

    const auto to_present = renderer::vulkan::makeImageBarrier(
        frame.colorImage(), colorSubresourceRange(), colorAttachmentWriteState(), presentState());
    commands.imageBarrier(to_present);
}

} // namespace arti::test_app
