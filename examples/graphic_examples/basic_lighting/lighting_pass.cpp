#include "lighting_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/render_device.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/nvrhi_shader_factory.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace arti::basic_lighting {
namespace {

struct LightingVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct LightingPushConstants {
    std::array<float, 16> model_view_projection;
    std::array<float, 16> model;
};

static_assert(std::is_standard_layout_v<LightingVertex>);
static_assert(std::is_standard_layout_v<LightingPushConstants>);
static_assert(sizeof(LightingPushConstants) == nvrhi::c_MaxPushConstantSize);

constexpr float kCubeExtent = 0.72f;
constexpr std::array kVertices = {
    LightingVertex{ { -kCubeExtent, -kCubeExtent, kCubeExtent }, { 0.0f, 0.0f, 1.0f } },
    LightingVertex{ { kCubeExtent, -kCubeExtent, kCubeExtent }, { 0.0f, 0.0f, 1.0f } },
    LightingVertex{ { kCubeExtent, kCubeExtent, kCubeExtent }, { 0.0f, 0.0f, 1.0f } },
    LightingVertex{ { -kCubeExtent, kCubeExtent, kCubeExtent }, { 0.0f, 0.0f, 1.0f } },

    LightingVertex{ { kCubeExtent, -kCubeExtent, -kCubeExtent }, { 0.0f, 0.0f, -1.0f } },
    LightingVertex{ { -kCubeExtent, -kCubeExtent, -kCubeExtent }, { 0.0f, 0.0f, -1.0f } },
    LightingVertex{ { -kCubeExtent, kCubeExtent, -kCubeExtent }, { 0.0f, 0.0f, -1.0f } },
    LightingVertex{ { kCubeExtent, kCubeExtent, -kCubeExtent }, { 0.0f, 0.0f, -1.0f } },

    LightingVertex{ { kCubeExtent, -kCubeExtent, kCubeExtent }, { 1.0f, 0.0f, 0.0f } },
    LightingVertex{ { kCubeExtent, -kCubeExtent, -kCubeExtent }, { 1.0f, 0.0f, 0.0f } },
    LightingVertex{ { kCubeExtent, kCubeExtent, -kCubeExtent }, { 1.0f, 0.0f, 0.0f } },
    LightingVertex{ { kCubeExtent, kCubeExtent, kCubeExtent }, { 1.0f, 0.0f, 0.0f } },

    LightingVertex{ { -kCubeExtent, -kCubeExtent, -kCubeExtent }, { -1.0f, 0.0f, 0.0f } },
    LightingVertex{ { -kCubeExtent, -kCubeExtent, kCubeExtent }, { -1.0f, 0.0f, 0.0f } },
    LightingVertex{ { -kCubeExtent, kCubeExtent, kCubeExtent }, { -1.0f, 0.0f, 0.0f } },
    LightingVertex{ { -kCubeExtent, kCubeExtent, -kCubeExtent }, { -1.0f, 0.0f, 0.0f } },

    LightingVertex{ { -kCubeExtent, kCubeExtent, kCubeExtent }, { 0.0f, 1.0f, 0.0f } },
    LightingVertex{ { kCubeExtent, kCubeExtent, kCubeExtent }, { 0.0f, 1.0f, 0.0f } },
    LightingVertex{ { kCubeExtent, kCubeExtent, -kCubeExtent }, { 0.0f, 1.0f, 0.0f } },
    LightingVertex{ { -kCubeExtent, kCubeExtent, -kCubeExtent }, { 0.0f, 1.0f, 0.0f } },

    LightingVertex{ { -kCubeExtent, -kCubeExtent, -kCubeExtent }, { 0.0f, -1.0f, 0.0f } },
    LightingVertex{ { kCubeExtent, -kCubeExtent, -kCubeExtent }, { 0.0f, -1.0f, 0.0f } },
    LightingVertex{ { kCubeExtent, -kCubeExtent, kCubeExtent }, { 0.0f, -1.0f, 0.0f } },
    LightingVertex{ { -kCubeExtent, -kCubeExtent, kCubeExtent }, { 0.0f, -1.0f, 0.0f } },
};

constexpr std::array<uint32_t, 36> kIndices = {
    0,
    1,
    2,
    2,
    3,
    0,
    4,
    5,
    6,
    6,
    7,
    4,
    8,
    9,
    10,
    10,
    11,
    8,
    12,
    13,
    14,
    14,
    15,
    12,
    16,
    17,
    18,
    18,
    19,
    16,
    20,
    21,
    22,
    22,
    23,
    20,
};

renderer::VertexBufferLayout lightingVertexLayout() {
    renderer::VertexBufferLayout layout;
    layout.stride = sizeof(LightingVertex);
    layout.attributes = {
        { 0, renderer::VertexAttributeType::Float3, offsetof(LightingVertex, position) },
        { 1, renderer::VertexAttributeType::Float3, offsetof(LightingVertex, normal) },
    };
    return layout;
}

} // namespace

struct LightingPass::Impl {
    Impl(renderer::RenderDevice& device, std::filesystem::path path)
            : shader_path(std::move(path)),
              vertex_buffer(device.createVertexBuffer(std::as_bytes(std::span{ kVertices }),
                      static_cast<uint32_t>(kVertices.size()), lightingVertexLayout())),
              index_buffer(device.createIndexBuffer(std::as_bytes(std::span{ kIndices }),
                      static_cast<uint32_t>(kIndices.size()))) {}

    std::filesystem::path shader_path;
    renderer::VertexBuffer vertex_buffer;
    renderer::IndexBuffer index_buffer;
    float rotation{ 0.0f };
    nvrhi::ShaderHandle vertex_shader;
    nvrhi::ShaderHandle pixel_shader;
    nvrhi::BindingLayoutHandle binding_layout;
    nvrhi::BindingSetHandle binding_set;
    nvrhi::InputLayoutHandle input_layout;
    nvrhi::GraphicsPipelineHandle pipeline;
    nvrhi::TextureHandle color_output;
    nvrhi::TextureHandle depth_output;
    nvrhi::FramebufferHandle framebuffer;
};

LightingPass::LightingPass(renderer::RenderDevice& device, std::filesystem::path shader_path)
        : m_impl(std::make_unique<Impl>(device, std::move(shader_path))) {}

LightingPass::~LightingPass() = default;

void LightingPass::setRotation(float radians) noexcept { m_impl->rotation = radians; }

void LightingPass::prepare(renderer::RenderPassPrepareContext& context) {
    if (!m_impl->binding_layout) {
        const renderer::CompiledGraphicsProgram program =
                renderer::SlangCompiler::compileGraphics({ m_impl->shader_path });
        const auto shaders = renderer::vulkan::createNvrhiGraphicsShaderSet(context.device(),
                program, "Basic Lighting geometry");
        if (shaders.binding_layouts.size() != 1 || !shaders.binding_layouts.front()) {
            throw std::runtime_error(
                    "The Basic Lighting shader requires one NVRHI binding layout.");
        }
        m_impl->vertex_shader = shaders.vertex_shader;
        m_impl->pixel_shader = shaders.pixel_shader;
        m_impl->binding_layout = shaders.binding_layouts.front();
        const std::array<renderer::vulkan::NvrhiBindingResource, 0> resources{};
        m_impl->binding_set = renderer::vulkan::createNvrhiBindingSet(context.device(),
                program.reflection, 0, *m_impl->binding_layout, resources);

        nvrhi::VertexAttributeDesc position;
        position.setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(LightingVertex, position))
                .setElementStride(sizeof(LightingVertex));
        nvrhi::VertexAttributeDesc normal;
        normal.setName("NORMAL")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(LightingVertex, normal))
                .setElementStride(sizeof(LightingVertex));
        const std::array attributes = { position, normal };
        m_impl->input_layout = context.device().createInputLayout(attributes.data(),
                attributes.size(), m_impl->vertex_shader);
        if (!m_impl->input_layout) {
            throw std::runtime_error("NVRHI failed to create the lighting input layout.");
        }
    }

    const uint32_t width = context.framebufferInfo().width;
    const uint32_t height = context.framebufferInfo().height;
    const bool size_changed = !m_impl->color_output ||
                              m_impl->color_output->getDesc().width != width ||
                              m_impl->color_output->getDesc().height != height;
    if (size_changed) {
        nvrhi::TextureDesc color_desc;
        color_desc.setWidth(width)
                .setHeight(height)
                .setFormat(nvrhi::Format::RGBA16_FLOAT)
                .setIsRenderTarget(true)
                .setDebugName("Basic Lighting HDR color")
                .enableAutomaticStateTracking(nvrhi::ResourceStates::ShaderResource);
        nvrhi::TextureDesc depth_desc;
        depth_desc.setWidth(width)
                .setHeight(height)
                .setFormat(nvrhi::Format::D32)
                .setIsRenderTarget(true)
                .setDebugName("Basic Lighting depth")
                .enableAutomaticStateTracking(nvrhi::ResourceStates::DepthWrite);
        m_impl->color_output = context.device().createTexture(color_desc);
        m_impl->depth_output = context.device().createTexture(depth_desc);
        if (!m_impl->color_output || !m_impl->depth_output) {
            throw std::runtime_error("NVRHI failed to create lighting attachments.");
        }
        nvrhi::FramebufferDesc framebuffer_desc;
        framebuffer_desc.addColorAttachment(m_impl->color_output)
                .setDepthAttachment(m_impl->depth_output);
        m_impl->framebuffer = context.device().createFramebuffer(framebuffer_desc);
        if (!m_impl->framebuffer) {
            throw std::runtime_error("NVRHI failed to create the lighting framebuffer.");
        }
        m_impl->pipeline = nullptr;
    }

    if (!m_impl->pipeline) {
        nvrhi::DepthStencilState depth_state;
        depth_state.enableDepthTest().enableDepthWrite().disableStencil();
        nvrhi::RasterState raster_state;
        raster_state.setCullBack().setFrontCounterClockwise(true);
        nvrhi::RenderState render_state;
        render_state.setDepthStencilState(depth_state).setRasterState(raster_state);
        nvrhi::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.setPrimType(nvrhi::PrimitiveType::TriangleList)
                .setInputLayout(m_impl->input_layout)
                .setVertexShader(m_impl->vertex_shader)
                .setPixelShader(m_impl->pixel_shader)
                .setRenderState(render_state)
                .addBindingLayout(m_impl->binding_layout);
        m_impl->pipeline = context.device().createGraphicsPipeline(pipeline_desc,
                m_impl->framebuffer->getFramebufferInfo());
        if (!m_impl->pipeline) {
            throw std::runtime_error("NVRHI failed to create the lighting pipeline.");
        }
    }
}

void LightingPass::record(renderer::RenderPassContext& context) {
    if (!m_impl->pipeline || !m_impl->framebuffer || !m_impl->binding_set) {
        throw std::logic_error("LightingPass was not prepared.");
    }

    auto& commands = context.commands();
    commands.clearTextureFloat(m_impl->color_output, nvrhi::AllSubresources,
            nvrhi::Color{ 0.018f, 0.025f, 0.04f, 1.0f });
    commands.clearDepthStencilTexture(m_impl->depth_output, nvrhi::AllSubresources, true, 1.0f,
            false, 0);

    const auto& framebuffer_info = m_impl->framebuffer->getFramebufferInfo();
    nvrhi::ViewportState viewport;
    viewport.addViewportAndScissorRect(framebuffer_info.getViewport());
    nvrhi::GraphicsState state;
    state.setPipeline(m_impl->pipeline)
            .setFramebuffer(m_impl->framebuffer)
            .setViewport(viewport)
            .addBindingSet(m_impl->binding_set)
            .addVertexBuffer(nvrhi::VertexBufferBinding()
                            .setBuffer(&context.buffer(m_impl->vertex_buffer))
                            .setSlot(0))
            .setIndexBuffer(nvrhi::IndexBufferBinding()
                            .setBuffer(&context.buffer(m_impl->index_buffer))
                            .setFormat(nvrhi::Format::R32_UINT));
    commands.setGraphicsState(state);

    constexpr glm::vec3 camera_position{ 4.8f, 3.2f, 6.4f };
    const float aspect = static_cast<float>(framebuffer_info.width) /
                         static_cast<float>(framebuffer_info.height);
    const glm::mat4 view =
            glm::lookAt(camera_position, glm::vec3{ 0.0f }, glm::vec3{ 0.0f, 1.0f, 0.0f });
    const glm::mat4 projection = glm::perspective(glm::radians(42.0f), aspect, 0.1f, 100.0f);
    const glm::mat4 view_projection = projection * view;

    const std::array translations = {
        glm::vec3{ -1.45f, -0.15f, -0.75f },
        glm::vec3{ 0.0f, 0.1f, 0.0f },
        glm::vec3{ 1.4f, -0.2f, 0.65f },
    };
    for (size_t index = 0; index < translations.size(); ++index) {
        glm::mat4 model{ 1.0f };
        model = glm::translate(model, translations[index]);
        model = glm::rotate(model,
                m_impl->rotation * (0.72f + 0.18f * index) + static_cast<float>(index) * 0.7f,
                glm::normalize(glm::vec3{ 0.35f + 0.2f * index, 1.0f, 0.2f }));
        LightingPushConstants push_constants{};
        const glm::mat4 mvp = view_projection * model;
        std::memcpy(push_constants.model_view_projection.data(), glm::value_ptr(mvp), sizeof(mvp));
        std::memcpy(push_constants.model.data(), glm::value_ptr(model), sizeof(model));
        commands.setPushConstants(&push_constants, sizeof(push_constants));
        commands.drawIndexed(
                nvrhi::DrawArguments{}.setVertexCount(m_impl->index_buffer.indexCount()));
    }
}

nvrhi::ITexture& LightingPass::colorOutput() const {
    if (!m_impl->color_output) {
        throw std::logic_error("LightingPass color output is not initialized.");
    }
    return *m_impl->color_output;
}

} // namespace arti::basic_lighting
