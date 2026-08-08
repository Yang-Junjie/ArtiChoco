#include "passes/common/indexed_graphics_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_binding_layout.h"
#include "artichoco/renderer/vulkan/vulkan_command_recorder.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline_cache.h"
#include "artichoco/renderer/vulkan/vulkan_resource_state.h"
#include "artichoco/renderer/vulkan/vulkan_shader.h"

#include <array>
#include <stdexcept>
#include <utility>

namespace arti::renderer_showcase {
namespace {

vk::ImageSubresourceRange colorRange()
{
    vk::ImageSubresourceRange range;
    range.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    return range;
}

vk::ImageSubresourceRange depthRange()
{
    vk::ImageSubresourceRange range;
    range.setAspectMask(vk::ImageAspectFlagBits::eDepth)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    return range;
}

renderer::vulkan::VulkanImageState undefinedState()
{
    return {vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone, vk::ImageLayout::eUndefined};
}

renderer::vulkan::VulkanImageState colorAttachmentState()
{
    return {vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eColorAttachmentOptimal};
}

renderer::vulkan::VulkanImageState depthAttachmentState()
{
    return {vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::ImageLayout::eDepthAttachmentOptimal};
}

renderer::vulkan::VulkanImageState presentState()
{
    return {vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone, vk::ImageLayout::ePresentSrcKHR};
}

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

struct IndexedGraphicsPass::Impl {
    Impl(renderer::VertexBuffer vertex_buffer,
         renderer::IndexBuffer index_buffer,
         std::filesystem::path shader_path,
         std::array<float, 4> clear_color,
         bool depth_test_enabled)
        : vertex_buffer(std::move(vertex_buffer)),
          index_buffer(std::move(index_buffer)),
          shader_path(std::move(shader_path)),
          clear_color(clear_color),
          depth_test_enabled(depth_test_enabled)
    {}

    renderer::VertexBuffer vertex_buffer;
    renderer::IndexBuffer index_buffer;
    std::filesystem::path shader_path;
    std::array<float, 4> clear_color;
    bool depth_test_enabled{false};
    float elapsed_time{0.0f};
    std::unique_ptr<renderer::vulkan::VulkanShader> shader;
    std::unique_ptr<renderer::vulkan::VulkanBindingLayout> binding_layout;
};

IndexedGraphicsPass::IndexedGraphicsPass(renderer::VertexBuffer vertex_buffer,
                                         renderer::IndexBuffer index_buffer,
                                         std::filesystem::path shader_path,
                                         std::array<float, 4> clear_color,
                                         bool depth_test_enabled)
    : m_impl(std::make_unique<Impl>(std::move(vertex_buffer),
                                    std::move(index_buffer),
                                    std::move(shader_path),
                                    clear_color,
                                    depth_test_enabled))
{}

IndexedGraphicsPass::~IndexedGraphicsPass() = default;

void IndexedGraphicsPass::setElapsedTime(float elapsed_time) noexcept
{
    m_impl->elapsed_time = elapsed_time;
}

void IndexedGraphicsPass::prepare(renderer::vulkan::VulkanPassPrepareContext& context)
{
    if (!m_impl->shader) {
        auto program = renderer::SlangCompiler::compileGraphics({m_impl->shader_path});
        m_impl->binding_layout =
            std::make_unique<renderer::vulkan::VulkanBindingLayout>(context.device(), program.reflection);
        m_impl->shader =
            std::make_unique<renderer::vulkan::VulkanShader>(context.device(), std::move(program));
    }
    prepareResources(context);
}

void IndexedGraphicsPass::record(renderer::vulkan::VulkanPassContext& context)
{
    auto& frame = context.frame();
    renderer::vulkan::VulkanGraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.color_formats = {frame.colorFormat()};
    if (m_impl->depth_test_enabled) {
        pipeline_info.depth_format = frame.depthFormat();
        pipeline_info.depth_test_enable = true;
        pipeline_info.depth_write_enable = true;
    }
    configurePipeline(pipeline_info);
    const auto& pipeline = context.pipelineCache().getGraphics(
        *m_impl->shader, *m_impl->binding_layout, m_impl->vertex_buffer.layout(), pipeline_info);

    auto& commands = context.commands();
    commands.imageBarrier(renderer::vulkan::makeImageBarrier(
        frame.colorImage(), colorRange(), undefinedState(), colorAttachmentState()));
    if (m_impl->depth_test_enabled) {
        commands.imageBarrier(renderer::vulkan::makeImageBarrier(
            frame.depthImage(), depthRange(), undefinedState(), depthAttachmentState()));
    }

    vk::RenderingAttachmentInfo color_attachment;
    color_attachment.setImageView(frame.colorImageView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearValue{vk::ClearColorValue{m_impl->clear_color}});
    vk::RenderingAttachmentInfo depth_attachment;
    if (m_impl->depth_test_enabled) {
        depth_attachment.setImageView(frame.depthImageView())
            .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setClearValue(vk::ClearValue{vk::ClearDepthStencilValue{1.0f, 0}});
    }
    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea(vk::Rect2D{{0, 0}, frame.extent()})
        .setLayerCount(1)
        .setColorAttachments(color_attachment);
    if (m_impl->depth_test_enabled) {
        rendering_info.setPDepthAttachment(&depth_attachment);
    }

    commands.beginRendering(rendering_info);
    commands.setViewportAndScissor(frame.extent());
    commands.bindPipeline(pipeline);
    bindResources(context, pipeline);
    commands.bindVertexBuffer(context.buffer(m_impl->vertex_buffer));
    commands.bindIndexBuffer(
        context.buffer(m_impl->index_buffer), toVulkanIndexType(m_impl->index_buffer.indexType()));
    commands.drawIndexed(m_impl->index_buffer.indexCount(), instanceCount());
    commands.endRendering();

    commands.imageBarrier(renderer::vulkan::makeImageBarrier(
        frame.colorImage(), colorRange(), colorAttachmentState(), presentState()));
}

void IndexedGraphicsPass::prepareResources(renderer::vulkan::VulkanPassPrepareContext&)
{}

void IndexedGraphicsPass::bindResources(renderer::vulkan::VulkanPassContext&,
                                        const renderer::vulkan::VulkanPipeline&)
{}

void IndexedGraphicsPass::configurePipeline(renderer::vulkan::VulkanGraphicsPipelineCreateInfo&) const
{}

uint32_t IndexedGraphicsPass::instanceCount() const noexcept
{
    return 1;
}

const renderer::vulkan::VulkanBindingLayout& IndexedGraphicsPass::bindingLayout() const
{
    if (!m_impl->binding_layout) {
        throw std::logic_error("The graphics pass has not been prepared.");
    }
    return *m_impl->binding_layout;
}

float IndexedGraphicsPass::elapsedTime() const noexcept
{
    return m_impl->elapsed_time;
}

} // namespace arti::renderer_showcase
