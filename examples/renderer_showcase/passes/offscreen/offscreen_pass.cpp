#include "passes/offscreen/offscreen_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_binding_layout.h"
#include "artichoco/renderer/vulkan/vulkan_command_recorder.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_image.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline_cache.h"
#include "artichoco/renderer/vulkan/vulkan_resource_state.h"
#include "artichoco/renderer/vulkan/vulkan_shader.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace arti::renderer_showcase {
namespace {

vk::IndexType toVulkanIndexType(renderer::IndexType type)
{
    switch (type) {
        case renderer::IndexType::UInt16:
            return vk::IndexType::eUint16;
        case renderer::IndexType::UInt32:
            return vk::IndexType::eUint32;
    }
    throw std::invalid_argument("Unsupported index type.");
}

} // namespace

struct OffscreenPass::Impl {
    Impl(renderer::VertexBuffer vertex_buffer,
         renderer::IndexBuffer index_buffer,
         std::filesystem::path shader_path)
        : vertex_buffer(std::move(vertex_buffer)),
          index_buffer(std::move(index_buffer)),
          shader_path(std::move(shader_path))
    {}

    renderer::VertexBuffer vertex_buffer;
    renderer::IndexBuffer index_buffer;
    std::filesystem::path shader_path;
    float elapsed_time{0.0f};
    std::unique_ptr<renderer::vulkan::VulkanImage> output;
    std::unique_ptr<renderer::vulkan::VulkanShader> shader;
    std::unique_ptr<renderer::vulkan::VulkanBindingLayout> binding_layout;
    bool output_initialized{false};
};

OffscreenPass::OffscreenPass(renderer::VertexBuffer vertex_buffer,
                             renderer::IndexBuffer index_buffer,
                             std::filesystem::path shader_path)
    : m_impl(std::make_unique<Impl>(
          std::move(vertex_buffer), std::move(index_buffer), std::move(shader_path)))
{}

OffscreenPass::~OffscreenPass() = default;

void OffscreenPass::setElapsedTime(float elapsed_time) noexcept
{
    m_impl->elapsed_time = elapsed_time;
}

void OffscreenPass::prepare(renderer::vulkan::VulkanPassPrepareContext& context)
{
    if (m_impl->shader) {
        return;
    }

    auto program = renderer::SlangCompiler::compileGraphics({m_impl->shader_path});
    m_impl->binding_layout =
        std::make_unique<renderer::vulkan::VulkanBindingLayout>(context.device(), program.reflection);
    m_impl->shader =
        std::make_unique<renderer::vulkan::VulkanShader>(context.device(), std::move(program));
    if (m_impl->binding_layout->pushConstantRanges().empty()) {
        throw std::invalid_argument("OffscreenPass requires a transform push constant.");
    }

    renderer::vulkan::VulkanImageCreateInfo image_info;
    image_info.extent = vk::Extent2D{640, 360};
    image_info.format = vk::Format::eR8G8B8A8Unorm;
    image_info.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    m_impl->output =
        std::make_unique<renderer::vulkan::VulkanImage>(context.device(), context.allocator(), image_info);
}

void OffscreenPass::record(renderer::vulkan::VulkanPassContext& context)
{
    renderer::vulkan::VulkanGraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.color_formats = {m_impl->output->format()};
    const auto& pipeline = context.pipelineCache().getGraphics(
        *m_impl->shader, *m_impl->binding_layout, m_impl->vertex_buffer.layout(), pipeline_info);

    const auto previous_state = m_impl->output_initialized
            ? renderer::vulkan::fragmentSampledReadState()
            : renderer::vulkan::undefinedImageState();
    context.commands().imageBarrier(renderer::vulkan::makeImageBarrier(
        m_impl->output->image(), vk::ImageAspectFlagBits::eColor, previous_state,
        renderer::vulkan::colorAttachmentWriteState()));

    vk::RenderingAttachmentInfo color_attachment;
    color_attachment.setImageView(*m_impl->output->imageView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearValue{vk::ClearColorValue{std::array{0.035f, 0.018f, 0.045f, 1.0f}}});
    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea(vk::Rect2D{{0, 0}, m_impl->output->extent()})
        .setLayerCount(1)
        .setColorAttachments(color_attachment);

    context.commands().beginRendering(rendering_info);
    context.commands().setViewportAndScissor(m_impl->output->extent());
    context.commands().bindPipeline(pipeline);
    const std::array transform = {
        std::cos(m_impl->elapsed_time),
        std::sin(m_impl->elapsed_time),
        0.82f,
        m_impl->elapsed_time,
    };
    const vk::PushConstantRange& range = m_impl->binding_layout->pushConstantRanges().front();
    context.commands().pushConstants(*pipeline.layout(), range.stageFlags, range.offset, transform);
    context.commands().bindVertexBuffer(context.buffer(m_impl->vertex_buffer));
    context.commands().bindIndexBuffer(
        context.buffer(m_impl->index_buffer), toVulkanIndexType(m_impl->index_buffer.indexType()));
    context.commands().drawIndexed(m_impl->index_buffer.indexCount());
    context.commands().endRendering();

    context.commands().imageBarrier(renderer::vulkan::makeImageBarrier(
        m_impl->output->image(), vk::ImageAspectFlagBits::eColor,
        renderer::vulkan::colorAttachmentWriteState(),
        renderer::vulkan::fragmentSampledReadState()));
    m_impl->output_initialized = true;
}

const renderer::vulkan::VulkanImage& OffscreenPass::output() const
{
    if (!m_impl->output) {
        throw std::logic_error("OffscreenPass must be prepared before its output is used.");
    }
    return *m_impl->output;
}

} // namespace arti::renderer_showcase
