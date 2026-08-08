#include "passes/compute_texture/compute_texture_pass.h"

#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vulkan/vulkan_binding_layout.h"
#include "artichoco/renderer/vulkan/vulkan_binding_set.h"
#include "artichoco/renderer/vulkan/vulkan_command_recorder.h"
#include "artichoco/renderer/vulkan/vulkan_compute_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_compute_shader.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_image.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline_cache.h"
#include "passes/common/pass_image_states.h"

#include <stdexcept>
#include <utility>
#include <vector>

namespace arti::renderer_showcase {

struct ComputeTexturePass::Impl {
    explicit Impl(std::filesystem::path shader_path)
        : shader_path(std::move(shader_path))
    {}

    std::filesystem::path shader_path;
    float elapsed_time{0.0f};
    std::unique_ptr<renderer::vulkan::VulkanImage> output;
    std::unique_ptr<renderer::vulkan::VulkanComputeShader> shader;
    std::unique_ptr<renderer::vulkan::VulkanBindingLayout> binding_layout;
    const renderer::vulkan::VulkanComputePipeline* pipeline{nullptr};
    std::vector<renderer::vulkan::VulkanBindingSet> binding_sets;
    bool output_initialized{false};
};

ComputeTexturePass::ComputeTexturePass(std::filesystem::path shader_path)
    : m_impl(std::make_unique<Impl>(std::move(shader_path)))
{}

ComputeTexturePass::~ComputeTexturePass() = default;

void ComputeTexturePass::setElapsedTime(float elapsed_time) noexcept
{
    m_impl->elapsed_time = elapsed_time;
}

void ComputeTexturePass::prepare(renderer::vulkan::VulkanPassPrepareContext& context)
{
    if (m_impl->shader) {
        return;
    }

    auto program = renderer::SlangCompiler::compileCompute({m_impl->shader_path});
    m_impl->binding_layout =
        std::make_unique<renderer::vulkan::VulkanBindingLayout>(context.device(), program.reflection);
    m_impl->shader =
        std::make_unique<renderer::vulkan::VulkanComputeShader>(context.device(), std::move(program));
    m_impl->pipeline = &context.pipelineCache().getCompute(*m_impl->shader, *m_impl->binding_layout);
    if (m_impl->binding_layout->pushConstantRanges().empty()) {
        throw std::invalid_argument("ComputeTexturePass requires a time push constant.");
    }

    renderer::vulkan::VulkanImageCreateInfo image_info;
    image_info.extent = vk::Extent2D{512, 512};
    image_info.format = vk::Format::eR8G8B8A8Unorm;
    image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
    m_impl->output =
        std::make_unique<renderer::vulkan::VulkanImage>(context.device(), context.allocator(), image_info);

    m_impl->binding_sets.reserve(context.frameSlotCount());
    for (size_t index = 0; index < context.frameSlotCount(); ++index) {
        m_impl->binding_sets.emplace_back(context.device(), context.descriptorAllocator(), *m_impl->binding_layout);
    }
}

void ComputeTexturePass::record(renderer::vulkan::VulkanPassContext& context)
{
    const auto previous_state = m_impl->output_initialized ? pass_image_states::fragmentSampledRead()
                                                            : pass_image_states::undefined();
    context.commands().imageBarrier(renderer::vulkan::makeImageBarrier(
        m_impl->output->image(),
        pass_image_states::colorRange(),
        previous_state,
        pass_image_states::computeStorageWrite()));

    auto& bindings = m_impl->binding_sets.at(context.frame().frameSlotIndex());
    bindings.writeStorageImage("output_texture", *m_impl->output->imageView());
    context.commands().bindPipeline(*m_impl->pipeline);
    context.commands().bindBindingSet(*m_impl->pipeline, bindings);
    const vk::PushConstantRange& range = m_impl->binding_layout->pushConstantRanges().front();
    context.commands().pushConstants(
        *m_impl->pipeline->layout(), range.stageFlags, range.offset, m_impl->elapsed_time);
    context.commands().dispatch(
        (m_impl->output->extent().width + m_impl->shader->groupSizeX() - 1) / m_impl->shader->groupSizeX(),
        (m_impl->output->extent().height + m_impl->shader->groupSizeY() - 1) / m_impl->shader->groupSizeY());

    context.commands().imageBarrier(renderer::vulkan::makeImageBarrier(
        m_impl->output->image(),
        pass_image_states::colorRange(),
        pass_image_states::computeStorageWrite(),
        pass_image_states::fragmentSampledRead()));
    m_impl->output_initialized = true;
}

const renderer::vulkan::VulkanImage& ComputeTexturePass::output() const
{
    if (!m_impl->output) {
        throw std::logic_error("ComputeTexturePass must be prepared before its output is used.");
    }
    return *m_impl->output;
}

} // namespace arti::renderer_showcase
