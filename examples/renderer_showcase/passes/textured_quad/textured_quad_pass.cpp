#include "passes/textured_quad/textured_quad_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/texture_2d.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_binding_set.h"
#include "artichoco/renderer/vulkan/vulkan_command_recorder.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_image.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_sampler.h"

#include <utility>
#include <vector>

namespace arti::renderer_showcase {

struct TexturedQuadPass::Impl {
    explicit Impl(renderer::Texture2D texture)
        : texture(std::move(texture))
    {}

    renderer::Texture2D texture;
    std::unique_ptr<renderer::vulkan::VulkanSampler> sampler;
    std::vector<renderer::vulkan::VulkanBindingSet> binding_sets;
};

TexturedQuadPass::TexturedQuadPass(renderer::VertexBuffer vertex_buffer,
                                   renderer::IndexBuffer index_buffer,
                                   renderer::Texture2D texture,
                                   std::filesystem::path shader_path)
    : IndexedGraphicsPass(std::move(vertex_buffer),
                          std::move(index_buffer),
                          std::move(shader_path),
                          {0.015f, 0.04f, 0.045f, 1.0f}),
      m_impl(std::make_unique<Impl>(std::move(texture)))
{}

TexturedQuadPass::~TexturedQuadPass() = default;

void TexturedQuadPass::prepareResources(renderer::vulkan::VulkanPassPrepareContext& context)
{
    if (m_impl->sampler) {
        return;
    }

    m_impl->sampler = std::make_unique<renderer::vulkan::VulkanSampler>(context.device());
    m_impl->binding_sets.reserve(context.frameSlotCount());
    for (size_t index = 0; index < context.frameSlotCount(); ++index) {
        m_impl->binding_sets.emplace_back(context.device(), context.descriptorAllocator(), bindingLayout());
    }
}

void TexturedQuadPass::bindResources(renderer::vulkan::VulkanPassContext& context,
                                     const renderer::vulkan::VulkanPipeline& pipeline)
{
    auto& bindings = m_impl->binding_sets.at(context.frame().frameSlotIndex());
    bindings.writeSampledImage("demo_texture", *context.image(m_impl->texture).imageView());
    bindings.writeSampler("demo_sampler", *m_impl->sampler->handle());
    context.commands().bindBindingSet(pipeline, bindings);
}

} // namespace arti::renderer_showcase
