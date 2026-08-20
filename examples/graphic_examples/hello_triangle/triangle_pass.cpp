#include "triangle_pass.h"

#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vulkan/vulkan_binding_layout.h"
#include "artichoco/renderer/vulkan/vulkan_command_recorder.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline_cache.h"
#include "artichoco/renderer/vulkan/vulkan_resource_state.h"
#include "artichoco/renderer/vulkan/vulkan_shader.h"

#include <array>
#include <utility>

namespace arti::hello_triangle {
namespace {

constexpr std::array<float, 4> clearColor = { 0.025f, 0.035f, 0.065f, 1.0f };

} // namespace

TrianglePass::TrianglePass(renderer::VertexBuffer vertex_buffer, std::filesystem::path shader_path)
        : m_vertex_buffer(std::move(vertex_buffer)),
          m_shader_path(std::move(shader_path)) {}

TrianglePass::~TrianglePass() = default;

void TrianglePass::prepare(renderer::vulkan::VulkanPassPrepareContext& context) {
    if (m_shader) {
        return;
    }

    auto program = renderer::SlangCompiler::compileGraphics({ m_shader_path });
    m_binding_layout = std::make_unique<renderer::vulkan::VulkanBindingLayout>(context.device(),
            program.reflection);
    m_shader =
            std::make_unique<renderer::vulkan::VulkanShader>(context.device(), std::move(program));
}

void TrianglePass::record(renderer::vulkan::VulkanPassContext& context) {
    auto& frame = context.frame();

    renderer::vulkan::VulkanGraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.color_formats = { frame.colorFormat() };
    const auto& pipeline = context.pipelineCache().getGraphics(*m_shader, *m_binding_layout,
            m_vertex_buffer.layout(), pipeline_info);

    auto& commands = context.commands();
    commands.imageBarrier(renderer::vulkan::makeImageBarrier(frame.colorImage(),
            vk::ImageAspectFlagBits::eColor, renderer::vulkan::undefinedImageState(),
            renderer::vulkan::colorAttachmentWriteState()));

    vk::RenderingAttachmentInfo color_attachment;
    color_attachment.setImageView(frame.colorImageView())
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearValue{ vk::ClearColorValue{ clearColor } });

    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea(vk::Rect2D{ { 0, 0 }, frame.extent() })
            .setLayerCount(1)
            .setColorAttachments(color_attachment);

    commands.beginRendering(rendering_info);
    commands.setViewportAndScissor(frame.extent());
    commands.bindPipeline(pipeline);
    commands.bindVertexBuffer(context.buffer(m_vertex_buffer));
    commands.draw(m_vertex_buffer.vertexCount());
    commands.endRendering();

    commands.imageBarrier(renderer::vulkan::makeImageBarrier(frame.colorImage(),
            vk::ImageAspectFlagBits::eColor, renderer::vulkan::colorAttachmentWriteState(),
            renderer::vulkan::presentState()));
}

} // namespace arti::hello_triangle
