#include "present_pass.h"

#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vulkan/nvrhi_shader_factory.h"
#include "lighting_pass.h"

#include <array>
#include <stdexcept>
#include <utility>

namespace arti::basic_lighting {

struct PresentPass::Impl {
    Impl(LightingPass& lighting_source, std::filesystem::path path)
            : source(&lighting_source),
              shader_path(std::move(path)) {}

    LightingPass* source{ nullptr };
    std::filesystem::path shader_path;
    renderer::ShaderReflection reflection;
    nvrhi::ShaderHandle vertex_shader;
    nvrhi::ShaderHandle pixel_shader;
    nvrhi::BindingLayoutHandle binding_layout;
    nvrhi::BindingSetHandle binding_set;
    nvrhi::GraphicsPipelineHandle pipeline;
    nvrhi::SamplerHandle sampler;
};

PresentPass::PresentPass(LightingPass& source, std::filesystem::path shader_path)
        : m_impl(std::make_unique<Impl>(source, std::move(shader_path))) {}

PresentPass::~PresentPass() = default;

void PresentPass::prepare(renderer::RenderPassPrepareContext& context) {
    if (!m_impl->binding_layout) {
        const renderer::CompiledGraphicsProgram program =
                renderer::SlangCompiler::compileGraphics({ m_impl->shader_path });
        const auto shaders = renderer::vulkan::createNvrhiGraphicsShaderSet(context.device(),
                program, "Basic Lighting present");
        if (shaders.binding_layouts.size() != 1 || !shaders.binding_layouts.front()) {
            throw std::runtime_error(
                    "The Basic Lighting present shader requires one binding layout.");
        }
        m_impl->reflection = program.reflection;
        m_impl->vertex_shader = shaders.vertex_shader;
        m_impl->pixel_shader = shaders.pixel_shader;
        m_impl->binding_layout = shaders.binding_layouts.front();
        m_impl->sampler = context.device().createSampler(nvrhi::SamplerDesc{});
        if (!m_impl->sampler) {
            throw std::runtime_error("NVRHI failed to create the present sampler.");
        }
    }

    const std::array resources = {
        renderer::vulkan::NvrhiBindingResource::Texture("lighting_output",
                m_impl->source->colorOutput()),
        renderer::vulkan::NvrhiBindingResource::Sampler("lighting_sampler", *m_impl->sampler),
    };
    m_impl->binding_set = renderer::vulkan::createNvrhiBindingSet(context.device(),
            m_impl->reflection, 0, *m_impl->binding_layout, resources);

    if (!m_impl->pipeline) {
        nvrhi::DepthStencilState depth_state;
        depth_state.disableDepthTest().disableDepthWrite().disableStencil();
        nvrhi::RasterState raster_state;
        raster_state.setCullNone();
        nvrhi::RenderState render_state;
        render_state.setDepthStencilState(depth_state).setRasterState(raster_state);
        nvrhi::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.setPrimType(nvrhi::PrimitiveType::TriangleList)
                .setVertexShader(m_impl->vertex_shader)
                .setPixelShader(m_impl->pixel_shader)
                .setRenderState(render_state)
                .addBindingLayout(m_impl->binding_layout);
        m_impl->pipeline =
                context.device().createGraphicsPipeline(pipeline_desc, context.framebufferInfo());
        if (!m_impl->pipeline) {
            throw std::runtime_error("NVRHI failed to create the present pipeline.");
        }
    }
}

void PresentPass::record(renderer::RenderPassContext& context) {
    if (!m_impl->pipeline || !m_impl->binding_set) {
        throw std::logic_error("PresentPass was not prepared.");
    }

    auto& commands = context.commands();
    commands.clearTextureFloat(&context.colorTexture(), nvrhi::AllSubresources,
            nvrhi::Color{ 0.015f, 0.02f, 0.03f, 1.0f });
    nvrhi::ViewportState viewport;
    viewport.addViewportAndScissorRect(context.framebufferInfo().getViewport());
    nvrhi::GraphicsState state;
    state.setPipeline(m_impl->pipeline)
            .setFramebuffer(&context.framebuffer())
            .setViewport(viewport)
            .addBindingSet(m_impl->binding_set);
    commands.setGraphicsState(state);
    commands.draw(nvrhi::DrawArguments{}.setVertexCount(3));
}

} // namespace arti::basic_lighting
