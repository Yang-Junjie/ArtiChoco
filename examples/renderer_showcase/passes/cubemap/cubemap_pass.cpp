#include "passes/cubemap/cubemap_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/texture_cube.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_binding_layout.h"
#include "artichoco/renderer/vulkan/vulkan_binding_set.h"
#include "artichoco/renderer/vulkan/vulkan_command_recorder.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_image.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_sampler.h"

#include <array>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace arti::renderer_showcase {
namespace {

struct CubemapPushConstants {
    std::array<float, 4> view;
};

static_assert(std::is_standard_layout_v<CubemapPushConstants>);
static_assert(sizeof(CubemapPushConstants) == sizeof(float) * 4);

} // namespace

struct CubemapPass::Impl {
    explicit Impl(renderer::TextureCube texture)
            : texture(std::move(texture)) {}

    renderer::TextureCube texture;
    std::unique_ptr<renderer::vulkan::VulkanSampler> sampler;
    std::vector<renderer::vulkan::VulkanBindingSet> binding_sets;
};

CubemapPass::CubemapPass(renderer::VertexBuffer vertex_buffer, renderer::IndexBuffer index_buffer,
        renderer::TextureCube texture, std::filesystem::path shader_path)
        : IndexedGraphicsPass(std::move(vertex_buffer), std::move(index_buffer),
                  std::move(shader_path), { 0.012f, 0.018f, 0.025f, 1.0f }),
          m_impl(std::make_unique<Impl>(std::move(texture))) {}

CubemapPass::~CubemapPass() = default;

void CubemapPass::prepareResources(renderer::vulkan::VulkanPassPrepareContext& context) {
    if (m_impl->sampler) {
        return;
    }
    if (bindingLayout().pushConstantRanges().empty()) {
        throw std::invalid_argument("CubemapPass requires a reflected push-constant range.");
    }

    renderer::vulkan::VulkanSamplerCreateInfo sampler_info;
    sampler_info.address_mode_u = vk::SamplerAddressMode::eClampToEdge;
    sampler_info.address_mode_v = vk::SamplerAddressMode::eClampToEdge;
    sampler_info.address_mode_w = vk::SamplerAddressMode::eClampToEdge;
    sampler_info.anisotropy_enable = false;
    sampler_info.max_lod = static_cast<float>(m_impl->texture.mipLevels() - 1);
    m_impl->sampler =
            std::make_unique<renderer::vulkan::VulkanSampler>(context.device(), sampler_info);

    m_impl->binding_sets.reserve(context.frameSlotCount());
    for (size_t index = 0; index < context.frameSlotCount(); ++index) {
        m_impl->binding_sets.emplace_back(context.device(), context.descriptorAllocator(),
                bindingLayout());
    }
}

void CubemapPass::bindResources(renderer::vulkan::VulkanPassContext& context,
        const renderer::vulkan::VulkanPipeline& pipeline) {
    auto& bindings = m_impl->binding_sets.at(context.frame().frameSlotIndex());
    bindings.writeSampledImage("demo_cubemap", *context.image(m_impl->texture).imageView());
    bindings.writeSampler("demo_sampler", *m_impl->sampler->handle());
    context.commands().bindBindingSet(pipeline, bindings);

    const CubemapPushConstants constants{ {
        elapsedTime(),
        static_cast<float>(m_impl->texture.mipLevels() - 1),
        0.0f,
        0.0f,
    } };
    const vk::PushConstantRange& range = bindingLayout().pushConstantRanges().front();
    context.commands().pushConstants(*pipeline.layout(), range.stageFlags, range.offset, constants);
}

} // namespace arti::renderer_showcase
