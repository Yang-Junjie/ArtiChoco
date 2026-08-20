#include "passes/render_to_cubemap/render_to_cubemap_pass.h"

#include "artichoco/renderer/index_buffer.h"
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

#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

namespace arti::renderer_showcase {
namespace {

struct CubemapPushConstants {
    std::array<float, 4> view;
};

vk::IndexType toVulkanIndexType(renderer::IndexType type) {
    switch (type) {
        case renderer::IndexType::UInt16:
            return vk::IndexType::eUint16;
        case renderer::IndexType::UInt32:
            return vk::IndexType::eUint32;
    }
    throw std::invalid_argument("Unsupported index type.");
}

} // namespace

struct RenderToCubemapPass::Impl {
    Impl(renderer::VertexBuffer vertex_buffer, renderer::IndexBuffer index_buffer,
            std::filesystem::path shader_path)
            : vertex_buffer(std::move(vertex_buffer)),
              index_buffer(std::move(index_buffer)),
              shader_path(std::move(shader_path)) {}

    renderer::VertexBuffer vertex_buffer;
    renderer::IndexBuffer index_buffer;
    std::filesystem::path shader_path;
    std::unique_ptr<renderer::vulkan::VulkanShader> shader;
    std::unique_ptr<renderer::vulkan::VulkanBindingLayout> binding_layout;
    std::unique_ptr<renderer::vulkan::VulkanSampler> sampler;
    std::vector<renderer::vulkan::VulkanBindingSet> binding_sets;
    renderer::vulkan::VulkanImage output;
    std::vector<vk::raii::ImageView> face_views;
    float elapsed_time{ 0.0f };
    bool output_initialized{ false };
};

RenderToCubemapPass::RenderToCubemapPass(renderer::VertexBuffer vertex_buffer,
        renderer::IndexBuffer index_buffer, std::filesystem::path shader_path)
        : m_impl(std::make_unique<Impl>(std::move(vertex_buffer), std::move(index_buffer),
                  std::move(shader_path))) {}

RenderToCubemapPass::~RenderToCubemapPass() = default;

void RenderToCubemapPass::setElapsedTime(float elapsed_time) noexcept {
    m_impl->elapsed_time = elapsed_time;
}

void RenderToCubemapPass::prepare(renderer::vulkan::VulkanPassPrepareContext& context) {
    if (!m_impl->shader) {
        auto program = renderer::SlangCompiler::compileGraphics({ m_impl->shader_path });
        m_impl->binding_layout = std::make_unique<renderer::vulkan::VulkanBindingLayout>(
                context.device(), program.reflection);
        m_impl->shader = std::make_unique<renderer::vulkan::VulkanShader>(context.device(),
                std::move(program));
    }
    if (m_impl->output.image()) {
        return;
    }
    if (m_impl->binding_layout->pushConstantRanges().empty()) {
        throw std::invalid_argument(
                "RenderToCubemapPass requires a reflected push-constant range.");
    }

    renderer::vulkan::VulkanImageCreateInfo image_info;
    image_info.extent = vk::Extent2D{ 64, 64 };
    image_info.format = vk::Format::eR16G16B16A16Sfloat;
    image_info.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    image_info.array_layers = 6;
    image_info.flags = vk::ImageCreateFlagBits::eCubeCompatible;
    image_info.view_type = vk::ImageViewType::eCube;
    m_impl->output =
            renderer::vulkan::VulkanImage{ context.device(), context.allocator(), image_info };
    m_impl->face_views.reserve(6);
    for (uint32_t layer = 0; layer < 6; ++layer) {
        m_impl->face_views.push_back(m_impl->output.createLayerView(context.device(), 0, layer));
    }

    renderer::vulkan::VulkanSamplerCreateInfo sampler_info;
    sampler_info.address_mode_u = vk::SamplerAddressMode::eClampToEdge;
    sampler_info.address_mode_v = vk::SamplerAddressMode::eClampToEdge;
    sampler_info.address_mode_w = vk::SamplerAddressMode::eClampToEdge;
    sampler_info.anisotropy_enable = false;
    m_impl->sampler =
            std::make_unique<renderer::vulkan::VulkanSampler>(context.device(), sampler_info);
    m_impl->binding_sets.reserve(context.frameSlotCount());
    for (size_t index = 0; index < context.frameSlotCount(); ++index) {
        m_impl->binding_sets.emplace_back(context.device(), context.descriptorAllocator(),
                *m_impl->binding_layout);
    }
}

void RenderToCubemapPass::record(renderer::vulkan::VulkanPassContext& context) {
    auto& frame = context.frame();
    renderer::vulkan::VulkanGraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.color_formats = { frame.colorFormat() };
    const auto& pipeline = context.pipelineCache().getGraphics(*m_impl->shader,
            *m_impl->binding_layout, m_impl->vertex_buffer.layout(), pipeline_info);
    auto& commands = context.commands();

    const auto before_state = m_impl->output_initialized
            ? renderer::vulkan::fragmentSampledReadState()
            : renderer::vulkan::undefinedImageState();
    commands.imageBarrier(renderer::vulkan::makeImageBarrier(m_impl->output.image(),
            vk::ImageAspectFlagBits::eColor, before_state,
            renderer::vulkan::colorAttachmentWriteState(), 1, 6));

    constexpr std::array<std::array<float, 4>, 6> clear_colors = { {
        { 4.0f, 0.12f, 0.08f, 1.0f },
        { 0.08f, 3.2f, 3.5f, 1.0f },
        { 0.1f, 4.2f, 0.14f, 1.0f },
        { 3.3f, 0.1f, 3.0f, 1.0f },
        { 0.12f, 0.35f, 4.5f, 1.0f },
        { 4.2f, 3.1f, 0.08f, 1.0f },
    } };
    for (uint32_t layer = 0; layer < m_impl->face_views.size(); ++layer) {
        vk::RenderingAttachmentInfo attachment;
        attachment.setImageView(*m_impl->face_views[layer])
                .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setClearValue(vk::ClearValue{ vk::ClearColorValue{ clear_colors[layer] } });
        vk::RenderingInfo rendering_info;
        rendering_info.setRenderArea(vk::Rect2D{ { 0, 0 }, m_impl->output.extent() })
                .setLayerCount(1)
                .setColorAttachments(attachment);
        commands.beginRendering(rendering_info);
        commands.endRendering();
    }
    commands.imageBarrier(renderer::vulkan::makeImageBarrier(m_impl->output.image(),
            vk::ImageAspectFlagBits::eColor, renderer::vulkan::colorAttachmentWriteState(),
            renderer::vulkan::fragmentSampledReadState(), 1, 6));
    m_impl->output_initialized = true;

    commands.imageBarrier(renderer::vulkan::makeImageBarrier(frame.colorImage(),
            vk::ImageAspectFlagBits::eColor, renderer::vulkan::undefinedImageState(),
            renderer::vulkan::colorAttachmentWriteState()));

    constexpr std::array<float, 4> swapchain_clear = { 0.01f, 0.02f, 0.03f, 1.0f };
    vk::RenderingAttachmentInfo color_attachment;
    color_attachment.setImageView(frame.colorImageView())
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearValue{ vk::ClearColorValue{ swapchain_clear } });
    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea(vk::Rect2D{ { 0, 0 }, frame.extent() })
            .setLayerCount(1)
            .setColorAttachments(color_attachment);
    commands.beginRendering(rendering_info);
    commands.setViewportAndScissor(frame.extent());
    commands.bindPipeline(pipeline);

    auto& bindings = m_impl->binding_sets.at(frame.frameSlotIndex());
    bindings.writeSampledImage("demo_cubemap", *m_impl->output.imageView());
    bindings.writeSampler("demo_sampler", *m_impl->sampler->handle());
    commands.bindBindingSet(pipeline, bindings);
    const CubemapPushConstants constants{ { m_impl->elapsed_time, 0.0f, 0.0f, 0.0f } };
    const auto& range = m_impl->binding_layout->pushConstantRanges().front();
    commands.pushConstants(*pipeline.layout(), range.stageFlags, range.offset, constants);
    commands.bindVertexBuffer(context.buffer(m_impl->vertex_buffer));
    commands.bindIndexBuffer(context.buffer(m_impl->index_buffer),
            toVulkanIndexType(m_impl->index_buffer.indexType()));
    commands.drawIndexed(m_impl->index_buffer.indexCount());
    commands.endRendering();
    commands.imageBarrier(renderer::vulkan::makeImageBarrier(frame.colorImage(),
            vk::ImageAspectFlagBits::eColor, renderer::vulkan::colorAttachmentWriteState(),
            renderer::vulkan::presentState()));
}

} // namespace arti::renderer_showcase
