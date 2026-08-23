#include "display_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/render_device.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/nvrhi_shader_factory.h"
#include "render_texture_pass.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace arti::render_to_texture {
namespace {

struct DisplayVertex {
    glm::vec3 position;
    glm::vec2 uv;
    glm::vec3 normal;
};

struct DisplayPushConstants {
    std::array<float, 16> model_view_projection;
    std::array<float, 16> model;
};

static_assert(std::is_standard_layout_v<DisplayVertex>);
static_assert(std::is_standard_layout_v<DisplayPushConstants>);
static_assert(sizeof(DisplayPushConstants) == nvrhi::c_MaxPushConstantSize);

constexpr float kCubeExtent = 0.86f;
constexpr std::array kVertices = {
    DisplayVertex{ { -kCubeExtent, -kCubeExtent, kCubeExtent }, { 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f } },
    DisplayVertex{ { kCubeExtent, -kCubeExtent, kCubeExtent }, { 1.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f } },
    DisplayVertex{ { kCubeExtent, kCubeExtent, kCubeExtent }, { 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f } },
    DisplayVertex{ { -kCubeExtent, kCubeExtent, kCubeExtent }, { 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f } },

    DisplayVertex{ { kCubeExtent, -kCubeExtent, -kCubeExtent }, { 0.0f, 1.0f },
        { 0.0f, 0.0f, -1.0f } },
    DisplayVertex{ { -kCubeExtent, -kCubeExtent, -kCubeExtent }, { 1.0f, 1.0f },
        { 0.0f, 0.0f, -1.0f } },
    DisplayVertex{ { -kCubeExtent, kCubeExtent, -kCubeExtent }, { 1.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f } },
    DisplayVertex{ { kCubeExtent, kCubeExtent, -kCubeExtent }, { 0.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f } },

    DisplayVertex{ { kCubeExtent, -kCubeExtent, kCubeExtent }, { 0.0f, 1.0f },
        { 1.0f, 0.0f, 0.0f } },
    DisplayVertex{ { kCubeExtent, -kCubeExtent, -kCubeExtent }, { 1.0f, 1.0f },
        { 1.0f, 0.0f, 0.0f } },
    DisplayVertex{ { kCubeExtent, kCubeExtent, -kCubeExtent }, { 1.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f } },
    DisplayVertex{ { kCubeExtent, kCubeExtent, kCubeExtent }, { 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f } },

    DisplayVertex{ { -kCubeExtent, -kCubeExtent, -kCubeExtent }, { 0.0f, 1.0f },
        { -1.0f, 0.0f, 0.0f } },
    DisplayVertex{ { -kCubeExtent, -kCubeExtent, kCubeExtent }, { 1.0f, 1.0f },
        { -1.0f, 0.0f, 0.0f } },
    DisplayVertex{ { -kCubeExtent, kCubeExtent, kCubeExtent }, { 1.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f } },
    DisplayVertex{ { -kCubeExtent, kCubeExtent, -kCubeExtent }, { 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f } },

    DisplayVertex{ { -kCubeExtent, kCubeExtent, kCubeExtent }, { 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f } },
    DisplayVertex{ { kCubeExtent, kCubeExtent, kCubeExtent }, { 1.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f } },
    DisplayVertex{ { kCubeExtent, kCubeExtent, -kCubeExtent }, { 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f } },
    DisplayVertex{ { -kCubeExtent, kCubeExtent, -kCubeExtent }, { 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f } },

    DisplayVertex{ { -kCubeExtent, -kCubeExtent, -kCubeExtent }, { 0.0f, 1.0f },
        { 0.0f, -1.0f, 0.0f } },
    DisplayVertex{ { kCubeExtent, -kCubeExtent, -kCubeExtent }, { 1.0f, 1.0f },
        { 0.0f, -1.0f, 0.0f } },
    DisplayVertex{ { kCubeExtent, -kCubeExtent, kCubeExtent }, { 1.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f } },
    DisplayVertex{ { -kCubeExtent, -kCubeExtent, kCubeExtent }, { 0.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f } },
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

renderer::VertexBufferLayout displayVertexLayout() {
    renderer::VertexBufferLayout layout;
    layout.stride = sizeof(DisplayVertex);
    layout.attributes = {
        { 0, renderer::VertexAttributeType::Float3, offsetof(DisplayVertex, position) },
        { 1, renderer::VertexAttributeType::Float2, offsetof(DisplayVertex, uv) },
        { 2, renderer::VertexAttributeType::Float3, offsetof(DisplayVertex, normal) },
    };
    return layout;
}

} // namespace

struct DisplayPass::Impl {
    struct BackBufferTarget {
        nvrhi::TextureHandle depth_buffer;
        nvrhi::FramebufferHandle framebuffer;
    };

    Impl(renderer::RenderDevice& device, RenderTexturePass& texture_source,
            std::filesystem::path path)
            : source(&texture_source),
              shader_path(std::move(path)),
              vertex_buffer(device.createVertexBuffer(std::as_bytes(std::span{ kVertices }),
                      static_cast<uint32_t>(kVertices.size()), displayVertexLayout())),
              index_buffer(device.createIndexBuffer(std::as_bytes(std::span{ kIndices }),
                      static_cast<uint32_t>(kIndices.size()))) {}

    RenderTexturePass* source{ nullptr };
    std::filesystem::path shader_path;
    renderer::VertexBuffer vertex_buffer;
    renderer::IndexBuffer index_buffer;
    float rotation{ 0.0f };
    renderer::ShaderReflection reflection;
    nvrhi::ShaderHandle vertex_shader;
    nvrhi::ShaderHandle pixel_shader;
    nvrhi::BindingLayoutHandle binding_layout;
    nvrhi::BindingSetHandle binding_set;
    nvrhi::InputLayoutHandle input_layout;
    nvrhi::GraphicsPipelineHandle pipeline;
    nvrhi::SamplerHandle sampler;
    nvrhi::TextureHandle current_depth_buffer;
    nvrhi::FramebufferHandle current_framebuffer;
    std::unordered_map<nvrhi::ITexture*, BackBufferTarget> back_buffer_targets;
    uint32_t width{ 0 };
    uint32_t height{ 0 };
    nvrhi::Format color_format{ nvrhi::Format::UNKNOWN };
};

DisplayPass::DisplayPass(renderer::RenderDevice& device, RenderTexturePass& source,
        std::filesystem::path shader_path)
        : m_impl(std::make_unique<Impl>(device, source, std::move(shader_path))) {}

DisplayPass::~DisplayPass() = default;

void DisplayPass::setRotation(float radians) noexcept { m_impl->rotation = radians; }

void DisplayPass::prepare(renderer::RenderPassPrepareContext& context) {
    if (!m_impl->binding_layout) {
        const renderer::CompiledGraphicsProgram program =
                renderer::SlangCompiler::compileGraphics({ m_impl->shader_path });
        const auto shaders = renderer::vulkan::createNvrhiGraphicsShaderSet(context.device(),
                program, "Render To Texture display");
        if (shaders.binding_layouts.size() != 1 || !shaders.binding_layouts.front()) {
            throw std::runtime_error("The display shader requires one binding layout.");
        }
        m_impl->reflection = program.reflection;
        m_impl->vertex_shader = shaders.vertex_shader;
        m_impl->pixel_shader = shaders.pixel_shader;
        m_impl->binding_layout = shaders.binding_layouts.front();

        nvrhi::VertexAttributeDesc position;
        position.setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(DisplayVertex, position))
                .setElementStride(sizeof(DisplayVertex));
        nvrhi::VertexAttributeDesc uv;
        uv.setName("TEXCOORD0")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(DisplayVertex, uv))
                .setElementStride(sizeof(DisplayVertex));
        nvrhi::VertexAttributeDesc normal;
        normal.setName("NORMAL")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(DisplayVertex, normal))
                .setElementStride(sizeof(DisplayVertex));
        const std::array attributes = { position, uv, normal };
        m_impl->input_layout = context.device().createInputLayout(attributes.data(),
                attributes.size(), m_impl->vertex_shader);
        nvrhi::SamplerDesc sampler_desc;
        sampler_desc.setAllFilters(true).setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
        m_impl->sampler = context.device().createSampler(sampler_desc);
        if (!m_impl->input_layout || !m_impl->sampler) {
            throw std::runtime_error("NVRHI failed to create display input resources.");
        }

        const std::array resources = {
            renderer::vulkan::NvrhiBindingResource::Texture("render_texture",
                    m_impl->source->output()),
            renderer::vulkan::NvrhiBindingResource::Sampler("render_texture_sampler",
                    *m_impl->sampler),
        };
        m_impl->binding_set = renderer::vulkan::createNvrhiBindingSet(context.device(),
                m_impl->reflection, 0, *m_impl->binding_layout, resources);
    }

    const auto& framebuffer_info = context.framebufferInfo();
    const nvrhi::Format color_format = framebuffer_info.colorFormats.front();
    const bool framebuffer_changed = m_impl->width != framebuffer_info.width ||
                                     m_impl->height != framebuffer_info.height ||
                                     m_impl->color_format != color_format;
    if (framebuffer_changed) {
        m_impl->back_buffer_targets.clear();
        m_impl->current_depth_buffer = nullptr;
        m_impl->current_framebuffer = nullptr;
        m_impl->pipeline = nullptr;
        m_impl->width = framebuffer_info.width;
        m_impl->height = framebuffer_info.height;
        m_impl->color_format = color_format;
    }

    auto* color_texture = context.framebuffer().getDesc().colorAttachments.front().texture;
    auto [iterator, inserted] = m_impl->back_buffer_targets.try_emplace(color_texture);
    if (inserted) {
        nvrhi::TextureDesc depth_desc;
        depth_desc.setWidth(framebuffer_info.width)
                .setHeight(framebuffer_info.height)
                .setFormat(nvrhi::Format::D32)
                .setIsRenderTarget(true)
                .setDebugName("Render To Texture back-buffer depth")
                .enableAutomaticStateTracking(nvrhi::ResourceStates::DepthWrite);
        iterator->second.depth_buffer = context.device().createTexture(depth_desc);
        if (!iterator->second.depth_buffer) {
            throw std::runtime_error("NVRHI failed to create a display depth buffer.");
        }
        nvrhi::FramebufferDesc framebuffer_desc;
        framebuffer_desc.addColorAttachment(color_texture)
                .setDepthAttachment(iterator->second.depth_buffer);
        iterator->second.framebuffer = context.device().createFramebuffer(framebuffer_desc);
        if (!iterator->second.framebuffer) {
            throw std::runtime_error("NVRHI failed to create a display framebuffer.");
        }
    }
    m_impl->current_depth_buffer = iterator->second.depth_buffer;
    m_impl->current_framebuffer = iterator->second.framebuffer;

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
                m_impl->current_framebuffer->getFramebufferInfo());
        if (!m_impl->pipeline) {
            throw std::runtime_error("NVRHI failed to create the display pipeline.");
        }
    }
}

void DisplayPass::record(renderer::RenderPassContext& context) {
    if (!m_impl->pipeline || !m_impl->current_depth_buffer || !m_impl->current_framebuffer ||
            !m_impl->binding_set) {
        throw std::logic_error("DisplayPass was not prepared.");
    }

    auto& commands = context.commands();
    commands.clearTextureFloat(&context.colorTexture(), nvrhi::AllSubresources,
            nvrhi::Color{ 0.022f, 0.03f, 0.045f, 1.0f });
    commands.clearDepthStencilTexture(m_impl->current_depth_buffer, nvrhi::AllSubresources, true,
            1.0f, false, 0);

    const auto& framebuffer_info = m_impl->current_framebuffer->getFramebufferInfo();
    nvrhi::ViewportState viewport;
    viewport.addViewportAndScissorRect(framebuffer_info.getViewport());
    nvrhi::GraphicsState state;
    state.setPipeline(m_impl->pipeline)
            .setFramebuffer(m_impl->current_framebuffer)
            .setViewport(viewport)
            .addBindingSet(m_impl->binding_set)
            .addVertexBuffer(nvrhi::VertexBufferBinding()
                            .setBuffer(&context.buffer(m_impl->vertex_buffer))
                            .setSlot(0))
            .setIndexBuffer(nvrhi::IndexBufferBinding()
                            .setBuffer(&context.buffer(m_impl->index_buffer))
                            .setFormat(nvrhi::Format::R32_UINT));
    commands.setGraphicsState(state);

    const float aspect = static_cast<float>(framebuffer_info.width) /
                         static_cast<float>(framebuffer_info.height);
    glm::mat4 model{ 1.0f };
    model = glm::rotate(model, m_impl->rotation, glm::vec3{ 0.0f, 1.0f, 0.0f });
    model = glm::rotate(model, m_impl->rotation * 0.57f, glm::vec3{ 1.0f, 0.0f, 0.0f });
    const glm::mat4 view = glm::lookAt(glm::vec3{ 2.75f, 2.15f, 3.35f }, glm::vec3{ 0.0f },
            glm::vec3{ 0.0f, 1.0f, 0.0f });
    const glm::mat4 projection = glm::perspective(glm::radians(44.0f), aspect, 0.1f, 100.0f);
    const glm::mat4 mvp = projection * view * model;
    DisplayPushConstants push_constants{};
    std::memcpy(push_constants.model_view_projection.data(), glm::value_ptr(mvp), sizeof(mvp));
    std::memcpy(push_constants.model.data(), glm::value_ptr(model), sizeof(model));
    commands.setPushConstants(&push_constants, sizeof(push_constants));
    commands.drawIndexed(nvrhi::DrawArguments{}.setVertexCount(m_impl->index_buffer.indexCount()));
}

} // namespace arti::render_to_texture
