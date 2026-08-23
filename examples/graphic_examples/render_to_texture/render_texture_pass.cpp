#include "render_texture_pass.h"

#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vulkan/nvrhi_shader_factory.h"

#include <array>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace arti::render_to_texture {
namespace {

constexpr uint32_t kRenderTextureSize = 512;

struct RenderTexturePushConstants {
    std::array<float, 4> animation;
};

static_assert(std::is_standard_layout_v<RenderTexturePushConstants>);
static_assert(sizeof(RenderTexturePushConstants) == sizeof(float) * 4);

} // namespace

struct RenderTexturePass::Impl {
    explicit Impl(std::filesystem::path path)
            : shader_path(std::move(path)) {}

    std::filesystem::path shader_path;
    float time{ 0.0f };
    nvrhi::ShaderHandle vertex_shader;
    nvrhi::ShaderHandle pixel_shader;
    nvrhi::BindingLayoutHandle binding_layout;
    nvrhi::BindingSetHandle binding_set;
    nvrhi::GraphicsPipelineHandle pipeline;
    nvrhi::TextureHandle output_texture;
    nvrhi::FramebufferHandle framebuffer;
};

RenderTexturePass::RenderTexturePass(std::filesystem::path shader_path)
        : m_impl(std::make_unique<Impl>(std::move(shader_path))) {}

RenderTexturePass::~RenderTexturePass() = default;

void RenderTexturePass::setTime(float seconds) noexcept { m_impl->time = seconds; }

void RenderTexturePass::prepare(renderer::RenderPassPrepareContext& context) {
    if (m_impl->pipeline) {
        return;
    }

    const renderer::CompiledGraphicsProgram program =
            renderer::SlangCompiler::compileGraphics({ m_impl->shader_path });
    const auto shaders = renderer::vulkan::createNvrhiGraphicsShaderSet(context.device(), program,
            "Render To Texture source");
    if (shaders.binding_layouts.size() != 1 || !shaders.binding_layouts.front()) {
        throw std::runtime_error("The render-texture shader requires one binding layout.");
    }
    m_impl->vertex_shader = shaders.vertex_shader;
    m_impl->pixel_shader = shaders.pixel_shader;
    m_impl->binding_layout = shaders.binding_layouts.front();
    const std::array<renderer::vulkan::NvrhiBindingResource, 0> resources{};
    m_impl->binding_set = renderer::vulkan::createNvrhiBindingSet(context.device(),
            program.reflection, 0, *m_impl->binding_layout, resources);

    nvrhi::TextureDesc texture_desc;
    texture_desc.setWidth(kRenderTextureSize)
            .setHeight(kRenderTextureSize)
            .setFormat(nvrhi::Format::RGBA8_UNORM)
            .setIsRenderTarget(true)
            .setDebugName("Render To Texture output")
            .enableAutomaticStateTracking(nvrhi::ResourceStates::ShaderResource);
    m_impl->output_texture = context.device().createTexture(texture_desc);
    if (!m_impl->output_texture) {
        throw std::runtime_error("NVRHI failed to create the render texture.");
    }
    nvrhi::FramebufferDesc framebuffer_desc;
    framebuffer_desc.addColorAttachment(m_impl->output_texture);
    m_impl->framebuffer = context.device().createFramebuffer(framebuffer_desc);
    if (!m_impl->framebuffer) {
        throw std::runtime_error("NVRHI failed to create the render-texture framebuffer.");
    }

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
    m_impl->pipeline = context.device().createGraphicsPipeline(pipeline_desc,
            m_impl->framebuffer->getFramebufferInfo());
    if (!m_impl->pipeline) {
        throw std::runtime_error("NVRHI failed to create the render-texture pipeline.");
    }
}

void RenderTexturePass::record(renderer::RenderPassContext& context) {
    if (!m_impl->pipeline || !m_impl->framebuffer || !m_impl->binding_set) {
        throw std::logic_error("RenderTexturePass was not prepared.");
    }

    auto& commands = context.commands();
    commands.clearTextureFloat(m_impl->output_texture, nvrhi::AllSubresources,
            nvrhi::Color{ 0.015f, 0.02f, 0.035f, 1.0f });
    nvrhi::ViewportState viewport;
    viewport.addViewportAndScissorRect(m_impl->framebuffer->getFramebufferInfo().getViewport());
    nvrhi::GraphicsState state;
    state.setPipeline(m_impl->pipeline)
            .setFramebuffer(m_impl->framebuffer)
            .setViewport(viewport)
            .addBindingSet(m_impl->binding_set);
    commands.setGraphicsState(state);
    RenderTexturePushConstants push_constants{};
    push_constants.animation[0] = m_impl->time;
    commands.setPushConstants(&push_constants, sizeof(push_constants));
    commands.draw(nvrhi::DrawArguments{}.setVertexCount(3));
}

nvrhi::ITexture& RenderTexturePass::output() const {
    if (!m_impl->output_texture) {
        throw std::logic_error("RenderTexturePass output is not initialized.");
    }
    return *m_impl->output_texture;
}

} // namespace arti::render_to_texture
