#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/texture_2d.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_binding_layout.h"
#include "artichoco/renderer/vulkan/vulkan_binding_set.h"
#include "artichoco/renderer/vulkan/vulkan_compute_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_compute_shader.h"
#include "artichoco/renderer/vulkan/vulkan_device.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_image.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_shader.h"
#include "render_passes.h"

#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

namespace arti::test_app {
namespace {

vk::ImageSubresourceRange colorSubresourceRange()
{
    vk::ImageSubresourceRange range{};
    range.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    return range;
}

vk::ImageSubresourceRange depthSubresourceRange()
{
    vk::ImageSubresourceRange range{};
    range.setAspectMask(vk::ImageAspectFlagBits::eDepth)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    return range;
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

void ThrowOncePass::record(renderer::vulkan::VulkanPassContext& context)
{
    if (m_did_throw) {
        return;
    }

    auto& frame = context.frame();
    vk::ImageMemoryBarrier2 to_color_attachment{};
    to_color_attachment.setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
        .setSrcAccessMask(vk::AccessFlagBits2::eNone)
        .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
        .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
        .setOldLayout(vk::ImageLayout::eUndefined)
        .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImage(frame.colorImage())
        .setSubresourceRange(colorSubresourceRange());

    auto& commands = context.commands();
    commands.imageBarrier(to_color_attachment);
    vk::RenderingAttachmentInfo color_attachment{};
    color_attachment.setImageView(frame.colorImageView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    vk::RenderingInfo rendering_info{};
    rendering_info.setRenderArea(vk::Rect2D{{0, 0}, frame.extent()})
        .setLayerCount(1)
        .setColorAttachments(color_attachment);
    commands.beginRendering(rendering_info);

    m_did_throw = true;
    throw std::runtime_error("Intentional Vulkan frame recording failure.");
}

bool ThrowOncePass::didThrow() const noexcept
{
    return m_did_throw;
}

struct TextureComputePass::Impl {
    Impl(const renderer::Texture2D& source, std::filesystem::path shader_path)
        : source(&source),
          shader_path(std::move(shader_path))
    {}

    void initialize(renderer::vulkan::VulkanPassPrepareContext& context)
    {
        if (pipeline) {
            return;
        }

        auto program = renderer::SlangCompiler::compileCompute(shader_path, "computeMain");
        shader = std::make_unique<renderer::vulkan::VulkanComputeShader>(context.device(),
                                                                         program.compute.spirv,
                                                                         std::move(program.compute.entry_point),
                                                                         program.thread_group_size_x,
                                                                         program.thread_group_size_y,
                                                                         program.thread_group_size_z);
        binding_layout = std::make_unique<renderer::vulkan::VulkanBindingLayout>(context.device(), program.reflection);
        pipeline =
            std::make_unique<renderer::vulkan::VulkanComputePipeline>(context.device(), *shader, *binding_layout);
        if (binding_layout->pushConstantRanges().empty()) {
            throw std::invalid_argument("The texture compute shader requires push constants.");
        }

        binding_sets.reserve(context.frameCount());
        for (size_t index = 0; index < context.frameCount(); ++index) {
            binding_sets.emplace_back(context.device(), context.descriptorAllocator(), *binding_layout);
        }
    }

    void ensureOutput(renderer::vulkan::VulkanPassPrepareContext& context)
    {
        const vk::Extent2D required_extent{source->width(), source->height()};
        if (output && output->extent() == required_extent) {
            return;
        }
        if (output) {
            context.device().device().waitIdle();
        }

        renderer::vulkan::VulkanImageCreateInfo image_info;
        image_info.extent = required_extent;
        image_info.format = vk::Format::eR8G8B8A8Unorm;
        image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
        image_info.create_sampler = true;
        output = std::make_unique<renderer::vulkan::VulkanImage>(context.device(), context.allocator(), image_info);
        output_initialized = false;
    }

    const renderer::Texture2D* source{nullptr};
    std::filesystem::path shader_path;
    std::unique_ptr<renderer::vulkan::VulkanComputeShader> shader;
    std::unique_ptr<renderer::vulkan::VulkanBindingLayout> binding_layout;
    std::unique_ptr<renderer::vulkan::VulkanComputePipeline> pipeline;
    std::unique_ptr<renderer::vulkan::VulkanImage> output;
    std::vector<renderer::vulkan::VulkanBindingSet> binding_sets;
    float time{0.0f};
    bool output_initialized{false};
};

TextureComputePass::TextureComputePass(const renderer::Texture2D& source, const std::filesystem::path& shader_path)
    : m_impl(std::make_unique<Impl>(source, shader_path))
{}

TextureComputePass::~TextureComputePass() = default;

void TextureComputePass::setSource(const renderer::Texture2D& source)
{
    m_impl->source = &source;
}

void TextureComputePass::setTime(float time) noexcept
{
    m_impl->time = time;
}

const renderer::vulkan::VulkanImage& TextureComputePass::output() const
{
    if (!m_impl->output) {
        throw std::logic_error("TextureComputePass must be prepared before its output is used.");
    }
    return *m_impl->output;
}

void TextureComputePass::prepare(renderer::vulkan::VulkanPassPrepareContext& context)
{
    m_impl->initialize(context);
    m_impl->ensureOutput(context);
}

void TextureComputePass::record(renderer::vulkan::VulkanPassContext& context)
{
    auto& frame = context.frame();
    const auto& source_image = context.image(*m_impl->source);
    auto& bindings = m_impl->binding_sets.at(frame.frameIndex());
    bindings.writeSampledImage("source_texture", *source_image.imageView());
    bindings.writeSampler("source_sampler", *source_image.sampler());
    bindings.writeStorageImage("output_texture", *m_impl->output->imageView());

    vk::ImageMemoryBarrier2 to_compute{};
    to_compute
        .setSrcStageMask(m_impl->output_initialized ? vk::PipelineStageFlagBits2::eFragmentShader
                                                    : vk::PipelineStageFlagBits2::eNone)
        .setSrcAccessMask(m_impl->output_initialized ? vk::AccessFlagBits2::eShaderSampledRead
                                                     : vk::AccessFlagBits2::eNone)
        .setDstStageMask(vk::PipelineStageFlagBits2::eComputeShader)
        .setDstAccessMask(vk::AccessFlagBits2::eShaderStorageWrite)
        .setOldLayout(m_impl->output_initialized ? vk::ImageLayout::eShaderReadOnlyOptimal
                                                 : vk::ImageLayout::eUndefined)
        .setNewLayout(vk::ImageLayout::eGeneral)
        .setImage(m_impl->output->image())
        .setSubresourceRange(colorSubresourceRange());

    auto& commands = context.commands();
    commands.imageBarrier(to_compute);
    commands.bindPipeline(*m_impl->pipeline);
    commands.bindBindingSet(*m_impl->pipeline, bindings);
    commands.pushConstants(
        *m_impl->pipeline->layout(), m_impl->binding_layout->pushConstantRanges().front().stageFlags, 0, m_impl->time);
    const vk::Extent2D extent = m_impl->output->extent();
    commands.dispatch((extent.width + m_impl->shader->groupSizeX() - 1) / m_impl->shader->groupSizeX(),
                      (extent.height + m_impl->shader->groupSizeY() - 1) / m_impl->shader->groupSizeY(),
                      1);

    vk::ImageMemoryBarrier2 to_graphics{};
    to_graphics.setSrcStageMask(vk::PipelineStageFlagBits2::eComputeShader)
        .setSrcAccessMask(vk::AccessFlagBits2::eShaderStorageWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
        .setDstAccessMask(vk::AccessFlagBits2::eShaderSampledRead)
        .setOldLayout(vk::ImageLayout::eGeneral)
        .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setImage(m_impl->output->image())
        .setSubresourceRange(colorSubresourceRange());
    commands.imageBarrier(to_graphics);
    m_impl->output_initialized = true;
}

struct TexturedMeshPass::Impl {
    Impl(TextureComputePass& texture_source, std::filesystem::path shader_path)
        : texture_source(&texture_source),
          shader_path(std::move(shader_path))
    {}

    void initialize(renderer::vulkan::VulkanPassPrepareContext& context)
    {
        if (shader) {
            return;
        }

        auto program = renderer::SlangCompiler::compileGraphics(shader_path, "vertexMain", "fragmentMain");
        shader = std::make_unique<renderer::vulkan::VulkanShader>(context.device(),
                                                                  program.vertex.spirv,
                                                                  std::move(program.vertex.entry_point),
                                                                  program.fragment.spirv,
                                                                  std::move(program.fragment.entry_point));
        binding_layout = std::make_unique<renderer::vulkan::VulkanBindingLayout>(context.device(), program.reflection);
        if (binding_layout->pushConstantRanges().empty()) {
            throw std::invalid_argument("The textured mesh shader requires push constants.");
        }

        binding_sets.reserve(context.frameCount());
        for (size_t index = 0; index < context.frameCount(); ++index) {
            binding_sets.emplace_back(context.device(), context.descriptorAllocator(), *binding_layout);
        }
        device = &context.device();
    }

    TextureComputePass* texture_source{nullptr};
    std::filesystem::path shader_path;
    const renderer::vulkan::VulkanDevice* device{nullptr};
    const renderer::VertexBuffer* vertex_buffer{nullptr};
    const renderer::IndexBuffer* index_buffer{nullptr};
    std::unique_ptr<renderer::vulkan::VulkanShader> shader;
    std::unique_ptr<renderer::vulkan::VulkanBindingLayout> binding_layout;
    std::unique_ptr<renderer::vulkan::VulkanPipeline> pipeline;
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
};

TexturedMeshPass::TexturedMeshPass(TextureComputePass& texture_source, const std::filesystem::path& shader_path)
    : m_impl(std::make_unique<Impl>(texture_source, shader_path))
{}

TexturedMeshPass::~TexturedMeshPass() = default;

void TexturedMeshPass::setGeometry(const renderer::VertexBuffer& vertex_buffer,
                                   const renderer::IndexBuffer& index_buffer) noexcept
{
    m_impl->vertex_buffer = &vertex_buffer;
    m_impl->index_buffer = &index_buffer;
}

void TexturedMeshPass::setTransform(const std::array<float, 16>& transform) noexcept
{
    m_impl->transform = transform;
}

void TexturedMeshPass::setClearColor(const std::array<float, 4>& color) noexcept
{
    m_impl->clear_color = color;
}

void TexturedMeshPass::prepare(renderer::vulkan::VulkanPassPrepareContext& context)
{
    m_impl->initialize(context);
    (void) m_impl->texture_source->output();
}

void TexturedMeshPass::record(renderer::vulkan::VulkanPassContext& context)
{
    if (m_impl->vertex_buffer == nullptr || m_impl->index_buffer == nullptr) {
        throw std::logic_error("TexturedMeshPass requires geometry before recording.");
    }

    auto& frame = context.frame();
    const auto& texture = m_impl->texture_source->output();
    const auto& layout = m_impl->vertex_buffer->layout();
    if (!m_impl->pipeline || m_impl->pipeline->vertexLayout() != layout ||
        m_impl->pipeline->colorFormat() != frame.colorFormat() ||
        m_impl->pipeline->depthFormat() != frame.depthFormat()) {
        m_impl->pipeline = std::make_unique<renderer::vulkan::VulkanPipeline>(*m_impl->device,
                                                                              *m_impl->shader,
                                                                              layout,
                                                                              *m_impl->binding_layout,
                                                                              frame.colorFormat(),
                                                                              frame.depthFormat());
    }

    auto& bindings = m_impl->binding_sets.at(frame.frameIndex());
    bindings.writeSampledImage("base_color_texture", *texture.imageView());
    bindings.writeSampler("base_color_sampler", *texture.sampler());

    vk::ImageMemoryBarrier2 to_color_attachment{};
    to_color_attachment.setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
        .setSrcAccessMask(vk::AccessFlagBits2::eNone)
        .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
        .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
        .setOldLayout(vk::ImageLayout::eUndefined)
        .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImage(frame.colorImage())
        .setSubresourceRange(colorSubresourceRange());
    vk::ImageMemoryBarrier2 to_depth_attachment{};
    to_depth_attachment.setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
        .setSrcAccessMask(vk::AccessFlagBits2::eNone)
        .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                         vk::PipelineStageFlagBits2::eLateFragmentTests)
        .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                          vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
        .setOldLayout(vk::ImageLayout::eUndefined)
        .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
        .setImage(frame.depthImage())
        .setSubresourceRange(depthSubresourceRange());
    const std::array attachment_barriers = {to_color_attachment, to_depth_attachment};

    auto& commands = context.commands();
    commands.pipelineBarrier({}, {}, attachment_barriers);
    vk::RenderingAttachmentInfo color_attachment{};
    color_attachment.setImageView(frame.colorImageView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearValue{vk::ClearColorValue{m_impl->clear_color}});
    vk::RenderingAttachmentInfo depth_attachment{};
    depth_attachment.setImageView(frame.depthImageView())
        .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setClearValue(vk::ClearValue{vk::ClearDepthStencilValue{1.0f, 0}});
    vk::RenderingInfo rendering_info{};
    rendering_info.setRenderArea(vk::Rect2D{{0, 0}, frame.extent()})
        .setLayerCount(1)
        .setColorAttachments(color_attachment)
        .setPDepthAttachment(&depth_attachment);

    commands.beginRendering(rendering_info);
    commands.setViewportAndScissor(frame.extent());
    commands.bindPipeline(*m_impl->pipeline);
    commands.bindBindingSet(*m_impl->pipeline, bindings);
    commands.bindVertexBuffer(context.buffer(*m_impl->vertex_buffer));
    commands.bindIndexBuffer(context.buffer(*m_impl->index_buffer),
                             toVulkanIndexType(m_impl->index_buffer->indexType()));
    commands.pushConstants(*m_impl->pipeline->layout(),
                           m_impl->binding_layout->pushConstantRanges().front().stageFlags,
                           0,
                           m_impl->transform);
    commands.drawIndexed(m_impl->index_buffer->indexCount());
    commands.endRendering();

    vk::ImageMemoryBarrier2 to_present{};
    to_present.setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
        .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eNone)
        .setDstAccessMask(vk::AccessFlagBits2::eNone)
        .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
        .setImage(frame.colorImage())
        .setSubresourceRange(colorSubresourceRange());
    commands.imageBarrier(to_present);
}

} // namespace arti::test_app
