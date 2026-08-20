#include "texture_compute_pass.h"

#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/texture_2d.h"
#include "artichoco/renderer/vulkan/vulkan_binding_layout.h"
#include "artichoco/renderer/vulkan/vulkan_binding_set.h"
#include "artichoco/renderer/vulkan/vulkan_compute_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_compute_shader.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_image.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline_cache.h"
#include "artichoco/renderer/vulkan/vulkan_resource_state.h"

#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

namespace arti::test_app {

struct TextureComputePass::Impl {
    Impl(std::shared_ptr<renderer::Texture2D> source, std::filesystem::path shader_path)
        : source(std::move(source)),
          shader_path(std::move(shader_path))
    {}

    void initialize(renderer::vulkan::VulkanPassPrepareContext& context)
    {
        if (pipeline) {
            return;
        }

        auto program = renderer::SlangCompiler::compileCompute({shader_path});
        binding_layout = std::make_unique<renderer::vulkan::VulkanBindingLayout>(context.device(), program.reflection);
        shader = std::make_unique<renderer::vulkan::VulkanComputeShader>(context.device(), std::move(program));
        pipeline = &context.pipelineCache().getCompute(*shader, *binding_layout);
        if (binding_layout->pushConstantRanges().empty()) {
            throw std::invalid_argument("The texture compute shader requires push constants.");
        }

        binding_sets.reserve(context.frameSlotCount());
        for (size_t index = 0; index < context.frameSlotCount(); ++index) {
            binding_sets.emplace_back(context.device(), context.descriptorAllocator(), *binding_layout);
        }
        input_sampler = renderer::vulkan::VulkanSampler{context.device()};
    }

    void ensureOutput(renderer::vulkan::VulkanPassPrepareContext& context)
    {
        if (!source) {
            return;
        }
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
        image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
                           vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
        image_info.mip_levels = renderer::vulkan::imageMipLevelCount(required_extent);
        output = std::make_unique<renderer::vulkan::VulkanImage>(context.device(), context.allocator(), image_info);
        output_level_zero_view = output->createLayerView(context.device(), 0, 0);
        output_initialized = false;
    }

    std::shared_ptr<renderer::Texture2D> source;
    std::filesystem::path shader_path;
    std::unique_ptr<renderer::vulkan::VulkanComputeShader> shader;
    std::unique_ptr<renderer::vulkan::VulkanBindingLayout> binding_layout;
    const renderer::vulkan::VulkanComputePipeline* pipeline{nullptr};
    renderer::vulkan::VulkanSampler input_sampler;
    std::unique_ptr<renderer::vulkan::VulkanImage> output;
    vk::raii::ImageView output_level_zero_view{nullptr};
    std::vector<renderer::vulkan::VulkanBindingSet> binding_sets;
    float time{0.0f};
    bool output_initialized{false};
};

TextureComputePass::TextureComputePass(std::shared_ptr<renderer::Texture2D> source,
                                       const std::filesystem::path& shader_path)
    : m_impl(std::make_unique<Impl>(std::move(source), shader_path))
{}

TextureComputePass::~TextureComputePass() = default;

void TextureComputePass::applyFrameData(const RenderFrameData& frame_data)
{
    m_impl->source = frame_data.draws.empty() ? nullptr : frame_data.draws.front().base_color_texture;
    m_impl->time = frame_data.time;
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
    if (!m_impl->source || !m_impl->output) {
        return;
    }
    auto& frame = context.frame();
    const auto& source_image = context.image(*m_impl->source);
    auto& bindings = m_impl->binding_sets.at(frame.frameSlotIndex());
    bindings.writeSampledImage("source_texture", *source_image.imageView());
    bindings.writeSampler("source_sampler", *m_impl->input_sampler.handle());
    bindings.writeStorageImage("output_texture", *m_impl->output_level_zero_view);

    const auto previous_state = m_impl->output_initialized
            ? renderer::vulkan::fragmentSampledReadState()
            : renderer::vulkan::undefinedImageState();
    const vk::ImageSubresourceRange all_mip_range =
        renderer::vulkan::fullImageRange(vk::ImageAspectFlagBits::eColor, m_impl->output->mipLevels());
    const auto to_transfer = renderer::vulkan::makeImageBarrier(
        m_impl->output->image(), all_mip_range, previous_state, renderer::vulkan::transferWriteState());
    const auto to_compute = renderer::vulkan::makeImageBarrier(
        m_impl->output->image(), vk::ImageAspectFlagBits::eColor, renderer::vulkan::transferWriteState(),
        renderer::vulkan::computeStorageWriteState());

    auto& commands = context.commands();
    commands.imageBarrier(to_transfer);
    commands.imageBarrier(to_compute);
    commands.bindPipeline(*m_impl->pipeline);
    commands.bindBindingSet(*m_impl->pipeline, bindings);
    commands.pushConstants(
        *m_impl->pipeline->layout(), m_impl->binding_layout->pushConstantRanges().front().stageFlags, 0, m_impl->time);
    const vk::Extent2D extent = m_impl->output->extent();
    commands.dispatch((extent.width + m_impl->shader->groupSizeX() - 1) / m_impl->shader->groupSizeX(),
                      (extent.height + m_impl->shader->groupSizeY() - 1) / m_impl->shader->groupSizeY(),
                      1);

    const auto to_mip_source = renderer::vulkan::makeImageBarrier(
        m_impl->output->image(), vk::ImageAspectFlagBits::eColor,
        renderer::vulkan::computeStorageWriteState(), renderer::vulkan::transferReadState());
    commands.imageBarrier(to_mip_source);
    commands.generateMipmaps(m_impl->output->image(), extent);

    const auto to_graphics = renderer::vulkan::makeImageBarrier(
        m_impl->output->image(), all_mip_range, renderer::vulkan::transferReadState(),
        renderer::vulkan::fragmentSampledReadState());
    commands.imageBarrier(to_graphics);
    m_impl->output_initialized = true;
}

} // namespace arti::test_app
