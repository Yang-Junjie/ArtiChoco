#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_binding_layout.h"
#include "artichoco/renderer/vulkan/vulkan_binding_set.h"
#include "artichoco/renderer/vulkan/vulkan_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_depth_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_device.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_image.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline_cache.h"
#include "artichoco/renderer/vulkan/vulkan_shader.h"
#include "artichoco/renderer/vulkan/vulkan_upload_context.h"
#include "mrt_mesh_pass.h"
#include "render_pass_common.h"

#include <cstddef>

#include <algorithm>
#include <array>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace arti::test_app {
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

struct MeshFrameUniforms {
    std::array<float, 16> model_view_projection;
    std::array<float, 4> tint;
};

static_assert(std::is_standard_layout_v<MeshFrameUniforms>);
static_assert(sizeof(MeshFrameUniforms) == sizeof(float) * 20);

constexpr std::array<float, 4> meshMaterialData = {1.0f, 0.94f, 0.86f, 1.0f};

bool supportsColorAttachmentSampling(const renderer::vulkan::VulkanDevice& device, vk::Format format)
{
    const vk::FormatFeatureFlags required =
        vk::FormatFeatureFlagBits::eColorAttachment | vk::FormatFeatureFlagBits::eSampledImage;
    const auto properties = device.physicalDevice().getFormatProperties(format);
    return (properties.optimalTilingFeatures & required) == required;
}

vk::Format chooseAuxiliaryFormat(const renderer::vulkan::VulkanDevice& device)
{
    constexpr std::array candidates = {
        vk::Format::eR16G16B16A16Sfloat,
        vk::Format::eR8G8B8A8Unorm,
    };
    for (const vk::Format format : candidates) {
        if (supportsColorAttachmentSampling(device, format)) {
            return format;
        }
    }
    throw std::runtime_error("The Vulkan device exposes no sampled color format for the MRT auxiliary output.");
}

vk::PipelineColorBlendAttachmentState opaqueColorBlend() noexcept
{
    vk::PipelineColorBlendAttachmentState blend;
    blend.setBlendEnable(false).setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                  vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    return blend;
}

} // namespace

struct MrtMeshPass::Impl {
    Impl(TextureComputePass& texture_source, std::filesystem::path shader_path)
        : texture_source(&texture_source),
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
        frame_uniform_buffers.reserve(context.frameSlotCount());

        renderer::vulkan::VulkanBufferCreateInfo uniform_buffer_info;
        uniform_buffer_info.size = sizeof(MeshFrameUniforms);
        uniform_buffer_info.usage = vk::BufferUsageFlagBits::eUniformBuffer;
        uniform_buffer_info.memory = renderer::vulkan::VulkanBufferMemory::HostVisible;
        
        for (size_t index = 0; index < context.frameSlotCount(); ++index) {
            frame_uniform_buffers.emplace_back(context.allocator(), uniform_buffer_info);
            binding_sets.emplace_back(context.device(), context.descriptorAllocator(), *binding_layout);
        }

        renderer::vulkan::VulkanBufferCreateInfo material_buffer_info;
        material_buffer_info.size = sizeof(meshMaterialData);
        material_buffer_info.usage = vk::BufferUsageFlagBits::eStorageBuffer;
        material_buffer_info.memory = renderer::vulkan::VulkanBufferMemory::DeviceLocal;
        material_buffer = std::make_unique<renderer::vulkan::VulkanBuffer>(context.allocator(), material_buffer_info);
        const renderer::vulkan::VulkanBufferState fragment_storage_read{
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderStorageRead,
        };
        material_buffer->uploadInitial(
            context.uploadContext(), std::as_bytes(std::span{meshMaterialData}), fragment_storage_read);

        for (size_t index = 0; index < binding_sets.size(); ++index) {
            binding_sets[index].writeUniformBuffer("frame_uniforms", frame_uniform_buffers[index]);
            binding_sets[index].writeStorageBuffer("material_data", *material_buffer);
        }
        device = &context.device();
    }

    void ensureAttachments(renderer::vulkan::VulkanPassPrepareContext& context)
    {
        const vk::Extent2D required_extent = texture_source->output().extent();
        if (color_output && color_output->extent() == required_extent) {
            return;
        }
        if (color_output) {
            context.device().device().waitIdle();
        }

        constexpr vk::Format color_format = vk::Format::eR8G8B8A8Unorm;
        if (!supportsColorAttachmentSampling(context.device(), color_format)) {
            throw std::runtime_error("The Vulkan device does not support the MRT color output format.");
        }

        renderer::vulkan::VulkanImageCreateInfo image_info;
        image_info.extent = required_extent;
        image_info.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
        image_info.create_sampler = true;
        image_info.format = color_format;

        color_output =
            std::make_unique<renderer::vulkan::VulkanImage>(context.device(), context.allocator(), image_info);

        image_info.format = chooseAuxiliaryFormat(context.device());
        auxiliary_output =
            std::make_unique<renderer::vulkan::VulkanImage>(context.device(), context.allocator(), image_info);

        depth_buffer = std::make_unique<renderer::vulkan::VulkanDepthBuffer>(
            context.device(), context.allocator(), required_extent);
        pipeline = nullptr;
        outputs_initialized = false;
        depth_initialized = false;
    }

    TextureComputePass* texture_source{nullptr};
    std::filesystem::path shader_path;
    const renderer::vulkan::VulkanDevice* device{nullptr};
    const renderer::VertexBuffer* vertex_buffer{nullptr};
    const renderer::IndexBuffer* index_buffer{nullptr};
    std::unique_ptr<renderer::vulkan::VulkanShader> shader;
    std::unique_ptr<renderer::vulkan::VulkanBindingLayout> binding_layout;
    const renderer::vulkan::VulkanPipeline* pipeline{nullptr};
    std::unique_ptr<renderer::vulkan::VulkanImage> color_output;
    std::unique_ptr<renderer::vulkan::VulkanImage> auxiliary_output;
    std::unique_ptr<renderer::vulkan::VulkanDepthBuffer> depth_buffer;
    std::unique_ptr<renderer::vulkan::VulkanBuffer> material_buffer;
    std::vector<renderer::vulkan::VulkanBuffer> frame_uniform_buffers;
    std::vector<renderer::vulkan::VulkanBindingSet> binding_sets;
    std::array<float, 16> transform{
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    };
    std::array<float, 4> clear_color{0.04f, 0.08f, 0.12f, 1.0f};
    bool outputs_initialized{false};
    bool depth_initialized{false};
};

MrtMeshPass::MrtMeshPass(TextureComputePass& texture_source, const std::filesystem::path& shader_path)
    : m_impl(std::make_unique<Impl>(texture_source, shader_path))
{}

MrtMeshPass::~MrtMeshPass() = default;

void MrtMeshPass::setGeometry(const renderer::VertexBuffer& vertex_buffer,
                              const renderer::IndexBuffer& index_buffer) noexcept
{
    m_impl->vertex_buffer = &vertex_buffer;
    m_impl->index_buffer = &index_buffer;
}

void MrtMeshPass::setTransform(const std::array<float, 16>& transform) noexcept
{
    m_impl->transform = transform;
}

void MrtMeshPass::setClearColor(const std::array<float, 4>& color) noexcept
{
    m_impl->clear_color = color;
}

const renderer::vulkan::VulkanImage& MrtMeshPass::colorOutput() const
{
    if (!m_impl->color_output) {
        throw std::logic_error("MrtMeshPass must be prepared before its color output is used.");
    }
    return *m_impl->color_output;
}

const renderer::vulkan::VulkanImage& MrtMeshPass::auxiliaryOutput() const
{
    if (!m_impl->auxiliary_output) {
        throw std::logic_error("MrtMeshPass must be prepared before its auxiliary output is used.");
    }
    return *m_impl->auxiliary_output;
}

void MrtMeshPass::prepare(renderer::vulkan::VulkanPassPrepareContext& context)
{
    m_impl->initialize(context);
    m_impl->ensureAttachments(context);
}

void MrtMeshPass::record(renderer::vulkan::VulkanPassContext& context)
{
    if (m_impl->vertex_buffer == nullptr || m_impl->index_buffer == nullptr) {
        throw std::logic_error("MrtMeshPass requires geometry before recording.");
    }

    auto& frame = context.frame();
    const auto& texture = m_impl->texture_source->output();
    const auto& layout = m_impl->vertex_buffer->layout();
    const std::array color_formats = {m_impl->color_output->format(), m_impl->auxiliary_output->format()};

    renderer::vulkan::VulkanGraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.color_formats.assign(color_formats.begin(), color_formats.end());
    pipeline_info.color_blend_attachments.assign(color_formats.size(), opaqueColorBlend());
    pipeline_info.depth_format = m_impl->depth_buffer->format();
    pipeline_info.depth_test_enable = true;
    pipeline_info.depth_write_enable = true;
    m_impl->pipeline =
        &context.pipelineCache().getGraphics(*m_impl->shader, *m_impl->binding_layout, layout, pipeline_info);

    auto& bindings = m_impl->binding_sets.at(frame.frameSlotIndex());
    MeshFrameUniforms frame_uniforms{
        m_impl->transform,
        {1.0f, 1.0f, 1.0f, 1.0f},
    };
    m_impl->frame_uniform_buffers.at(frame.frameSlotIndex()).write(std::as_bytes(std::span{&frame_uniforms, 1}));
    bindings.writeSampledImage("base_color_texture", *texture.imageView());
    bindings.writeSampler("base_color_sampler", *texture.sampler());

    const auto previous_output_state = m_impl->outputs_initialized ? fragmentSampledReadState() : undefinedImageState();
    const auto previous_depth_state =
        m_impl->depth_initialized ? depthAttachmentReadWriteState() : undefinedImageState();
    const auto to_color_attachment = renderer::vulkan::makeImageBarrier(
        m_impl->color_output->image(), colorSubresourceRange(), previous_output_state, colorAttachmentWriteState());
    const auto to_auxiliary_attachment = renderer::vulkan::makeImageBarrier(
        m_impl->auxiliary_output->image(), colorSubresourceRange(), previous_output_state, colorAttachmentWriteState());
    const auto to_depth_attachment = renderer::vulkan::makeImageBarrier(
        m_impl->depth_buffer->image(), depthSubresourceRange(), previous_depth_state, depthAttachmentReadWriteState());
    const std::array attachment_barriers = {
        to_color_attachment,
        to_auxiliary_attachment,
        to_depth_attachment,
    };

    auto& commands = context.commands();
    commands.pipelineBarrier({}, {}, attachment_barriers);
    std::array<vk::RenderingAttachmentInfo, 2> color_attachments;
    color_attachments[0]
        .setImageView(*m_impl->color_output->imageView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearValue{vk::ClearColorValue{m_impl->clear_color}});
    color_attachments[1]
        .setImageView(*m_impl->auxiliary_output->imageView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearValue{vk::ClearColorValue{std::array{0.08f, 0.12f, 0.16f, 1.0f}}});
    vk::RenderingAttachmentInfo depth_attachment{};
    depth_attachment.setImageView(*m_impl->depth_buffer->imageView())
        .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setClearValue(vk::ClearValue{vk::ClearDepthStencilValue{1.0f, 0}});
    vk::RenderingInfo rendering_info{};
    rendering_info.setRenderArea(vk::Rect2D{{0, 0}, m_impl->color_output->extent()})
        .setLayerCount(1)
        .setColorAttachments(color_attachments)
        .setPDepthAttachment(&depth_attachment);

    commands.beginRendering(rendering_info);
    commands.setViewportAndScissor(m_impl->color_output->extent());
    commands.bindPipeline(*m_impl->pipeline);
    commands.bindBindingSet(*m_impl->pipeline, bindings);
    commands.bindVertexBuffer(context.buffer(*m_impl->vertex_buffer));
    commands.bindIndexBuffer(context.buffer(*m_impl->index_buffer),
                             toVulkanIndexType(m_impl->index_buffer->indexType()));
    commands.drawIndexed(m_impl->index_buffer->indexCount());
    commands.endRendering();

    const auto color_to_sample = renderer::vulkan::makeImageBarrier(m_impl->color_output->image(),
                                                                    colorSubresourceRange(),
                                                                    colorAttachmentWriteState(),
                                                                    fragmentSampledReadState());
    const auto auxiliary_to_sample = renderer::vulkan::makeImageBarrier(m_impl->auxiliary_output->image(),
                                                                        colorSubresourceRange(),
                                                                        colorAttachmentWriteState(),
                                                                        fragmentSampledReadState());
    const std::array sample_barriers = {color_to_sample, auxiliary_to_sample};
    commands.pipelineBarrier({}, {}, sample_barriers);
    m_impl->outputs_initialized = true;
    m_impl->depth_initialized = true;
}

} // namespace arti::test_app
