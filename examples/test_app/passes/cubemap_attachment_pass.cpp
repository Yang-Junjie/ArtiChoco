#include "cubemap_attachment_pass.h"

#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vulkan/nvrhi_shader_factory.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace arti::test_app {
namespace {

constexpr uint32_t kFaceSize = 16;
constexpr uint32_t kVerifiedFace = 5;
const std::array<nvrhi::Color, 6> kFaceColors = {
    nvrhi::Color{1.0f, 0.0f, 0.0f, 1.0f},
    nvrhi::Color{0.0f, 1.0f, 0.0f, 1.0f},
    nvrhi::Color{0.0f, 0.0f, 1.0f, 1.0f},
    nvrhi::Color{1.0f, 1.0f, 0.0f, 1.0f},
    nvrhi::Color{1.0f, 0.0f, 1.0f, 1.0f},
    nvrhi::Color{0.25f, 0.5f, 0.75f, 1.0f},
};

bool nearByte(uint8_t actual, float expected)
{
    return std::abs(static_cast<int>(actual) -
            static_cast<int>(std::lround(expected * 255.0f))) <= 1;
}

} // namespace

struct CubemapAttachmentPass::Impl {
    std::filesystem::path shader_path;
    nvrhi::IDevice* device{nullptr};
    nvrhi::TextureHandle cube_texture;
    nvrhi::StagingTextureHandle readback_texture;
    std::array<nvrhi::FramebufferHandle, 6> framebuffers;
    nvrhi::GraphicsPipelineHandle pipeline;
    nvrhi::BindingSetHandle binding_set;
    std::array<uint8_t, 4> readback_pixel{};
    bool recorded{false};
};

CubemapAttachmentPass::CubemapAttachmentPass(std::filesystem::path shader_path)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->shader_path = std::move(shader_path);
}

CubemapAttachmentPass::~CubemapAttachmentPass() = default;

void CubemapAttachmentPass::prepare(renderer::RenderPassPrepareContext& context)
{
    if (m_impl->pipeline) {
        return;
    }
    m_impl->device = &context.device();

    nvrhi::TextureDesc cube_desc;
    cube_desc.setWidth(kFaceSize)
            .setHeight(kFaceSize)
            .setArraySize(6)
            .setMipLevels(1)
            .setDimension(nvrhi::TextureDimension::TextureCube)
            .setFormat(nvrhi::Format::RGBA8_UNORM)
            .setDebugName("ArtiChoco cubemap attachment smoke")
            .setIsRenderTarget(true)
            .enableAutomaticStateTracking(nvrhi::ResourceStates::ShaderResource);
    m_impl->cube_texture = context.device().createTexture(cube_desc);

    nvrhi::TextureDesc readback_desc = cube_desc;
    readback_desc.setDimension(nvrhi::TextureDimension::Texture2DArray)
            .setDebugName("ArtiChoco cubemap attachment readback");
    readback_desc.isRenderTarget = false;
    readback_desc.keepInitialState = false;
    readback_desc.initialState = nvrhi::ResourceStates::Unknown;
    m_impl->readback_texture = context.device().createStagingTexture(
            readback_desc, nvrhi::CpuAccessMode::Read);
    if (!m_impl->cube_texture || !m_impl->readback_texture) {
        throw std::runtime_error("NVRHI failed to create cubemap attachment resources.");
    }

    for (uint32_t face = 0; face < m_impl->framebuffers.size(); ++face) {
        const nvrhi::TextureSubresourceSet face_subresources{0, 1, face, 1};
        nvrhi::FramebufferDesc framebuffer_desc;
        framebuffer_desc.addColorAttachment(m_impl->cube_texture, face_subresources);
        m_impl->framebuffers[face] = context.device().createFramebuffer(framebuffer_desc);
        if (!m_impl->framebuffers[face]) {
            throw std::runtime_error("NVRHI failed to create a cubemap face framebuffer.");
        }
    }

    const renderer::CompiledGraphicsProgram program =
            renderer::SlangCompiler::compileGraphics({m_impl->shader_path});
    const auto shaders = renderer::vulkan::createNvrhiGraphicsShaderSet(
            context.device(), program, "ArtiChoco cubemap attachment smoke");
    if (shaders.binding_layouts.size() != 1 || !shaders.binding_layouts.front()) {
        throw std::runtime_error(
                "The cubemap attachment shader requires one NVRHI binding layout.");
    }
    const std::array<renderer::vulkan::NvrhiBindingResource, 0> resources{};
    m_impl->binding_set = renderer::vulkan::createNvrhiBindingSet(
            context.device(), program.reflection, 0,
            *shaders.binding_layouts.front(), resources);
    nvrhi::DepthStencilState depth_state;
    depth_state.disableDepthTest().disableDepthWrite().disableStencil();
    nvrhi::RasterState raster_state;
    raster_state.setCullNone();
    nvrhi::RenderState render_state;
    render_state.setDepthStencilState(depth_state).setRasterState(raster_state);
    nvrhi::GraphicsPipelineDesc pipeline_desc;
    pipeline_desc.setPrimType(nvrhi::PrimitiveType::TriangleList)
            .setVertexShader(shaders.vertex_shader)
            .setPixelShader(shaders.pixel_shader)
            .setRenderState(render_state);
    for (const nvrhi::BindingLayoutHandle& layout : shaders.binding_layouts) {
        if (layout) {
            pipeline_desc.addBindingLayout(layout);
        }
    }
    m_impl->pipeline = context.device().createGraphicsPipeline(
            pipeline_desc, m_impl->framebuffers.front()->getFramebufferInfo());
    if (!m_impl->pipeline) {
        throw std::runtime_error("NVRHI failed to create the cubemap attachment pipeline.");
    }
}

void CubemapAttachmentPass::record(renderer::RenderPassContext& context)
{
    if (!m_impl->pipeline || m_impl->recorded) {
        return;
    }

    for (uint32_t face = 0; face < m_impl->framebuffers.size(); ++face) {
        nvrhi::ViewportState viewport;
        viewport.addViewportAndScissorRect(
                m_impl->framebuffers[face]->getFramebufferInfo().getViewport());
        nvrhi::GraphicsState state;
        state.setPipeline(m_impl->pipeline)
                .setFramebuffer(m_impl->framebuffers[face])
                .setViewport(viewport)
                .addBindingSet(m_impl->binding_set);
        context.commands().setGraphicsState(state);
        const nvrhi::Color color = kFaceColors[face];
        context.commands().setPushConstants(&color, sizeof(color));
        context.commands().draw(nvrhi::DrawArguments{}.setVertexCount(3));
    }

    const nvrhi::TextureSlice verified_face =
            nvrhi::TextureSlice().setArraySlice(kVerifiedFace);
    context.commands().copyTexture(
            m_impl->readback_texture, verified_face, m_impl->cube_texture, verified_face);
    context.commands().setPermanentTextureState(
            m_impl->cube_texture, nvrhi::ResourceStates::ShaderResource);
    m_impl->recorded = true;
}

bool CubemapAttachmentPass::verifyReadback()
{
    if (!m_impl->recorded || m_impl->device == nullptr) {
        return false;
    }
    const nvrhi::TextureSlice verified_face =
            nvrhi::TextureSlice().setArraySlice(kVerifiedFace);
    size_t row_pitch = 0;
    const auto* pixel = static_cast<const uint8_t*>(m_impl->device->mapStagingTexture(
            m_impl->readback_texture, verified_face, nvrhi::CpuAccessMode::Read, &row_pitch));
    const auto& expected = kFaceColors[kVerifiedFace];
    if (pixel != nullptr && row_pitch >= m_impl->readback_pixel.size()) {
        std::copy_n(pixel, m_impl->readback_pixel.size(), m_impl->readback_pixel.begin());
    }
    const bool valid = pixel != nullptr && row_pitch >= 4 &&
            nearByte(m_impl->readback_pixel[0], expected.r) &&
            nearByte(m_impl->readback_pixel[1], expected.g) &&
            nearByte(m_impl->readback_pixel[2], expected.b) &&
            nearByte(m_impl->readback_pixel[3], expected.a);
    if (pixel != nullptr) {
        m_impl->device->unmapStagingTexture(m_impl->readback_texture);
    }
    return valid;
}

std::array<uint8_t, 4> CubemapAttachmentPass::readbackPixel() const noexcept
{
    return m_impl->readback_pixel;
}

} // namespace arti::test_app
