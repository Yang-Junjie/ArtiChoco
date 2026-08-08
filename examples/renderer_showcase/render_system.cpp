#include "render_system.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/render_device.h"
#include "artichoco/renderer/texture_2d.h"
#include "artichoco/renderer/texture_cube.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_pass.h"
#include "image_loader.h"
#include "passes/alpha_blending/alpha_blending_pass.h"
#include "passes/common/image_display_pass.h"
#include "passes/common/showcase_pass.h"
#include "passes/compute_texture/compute_texture_pass.h"
#include "passes/cubemap/cubemap_pass.h"
#include "passes/depth_test/depth_test_pass.h"
#include "passes/instancing/instancing_pass.h"
#include "passes/offscreen/offscreen_pass.h"
#include "passes/push_constants/push_constants_pass.h"
#include "passes/render_to_cubemap/render_to_cubemap_pass.h"
#include "passes/textured_quad/textured_quad_pass.h"
#include "passes/triangle/triangle_pass.h"
#include "passes/uniform_buffer/uniform_buffer_pass.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace arti::renderer_showcase {
namespace {

struct ColorVertex {
    glm::vec3 position;
    glm::vec3 color;
};

struct TexturedVertex {
    glm::vec3 position;
    glm::vec2 uv;
};

struct BlendVertex {
    glm::vec3 position;
    glm::vec4 color;
};

static_assert(std::is_standard_layout_v<ColorVertex>);
static_assert(std::is_standard_layout_v<TexturedVertex>);
static_assert(std::is_standard_layout_v<BlendVertex>);

constexpr std::array triangleVertices = {
    ColorVertex{ { -0.75f, -0.65f, 0.0f }, { 1.0f, 0.2f, 0.15f } },
    ColorVertex{ { 0.75f, -0.65f, 0.0f }, { 0.2f, 1.0f, 0.25f } },
    ColorVertex{ { 0.0f, 0.75f, 0.0f }, { 0.2f, 0.45f, 1.0f } },
};
constexpr std::array<uint16_t, 3> triangleIndices = { 0, 1, 2 };

constexpr std::array texturedQuadVertices = {
    TexturedVertex{ { -0.72f, -0.72f, 0.0f }, { 0.0f, 1.0f } },
    TexturedVertex{ { 0.72f, -0.72f, 0.0f }, { 1.0f, 1.0f } },
    TexturedVertex{ { 0.72f, 0.72f, 0.0f }, { 1.0f, 0.0f } },
    TexturedVertex{ { -0.72f, 0.72f, 0.0f }, { 0.0f, 0.0f } },
};
constexpr std::array<uint16_t, 6> quadIndices = { 0, 1, 2, 2, 3, 0 };
constexpr uint32_t cubemapFaceSize = 64;
constexpr uint32_t cubemapMipLevels = 4;

struct CubemapMipPixels {
    uint32_t size{ 0 };
    std::array<std::vector<std::byte>, 6> faces;
};

uint16_t floatToHalf(float value) {
    const uint32_t bits = std::bit_cast<uint32_t>(value);
    const uint32_t sign = (bits >> 16) & 0x8000u;
    uint32_t mantissa = bits & 0x007fffffu;
    int32_t exponent = static_cast<int32_t>((bits >> 23) & 0xffu) - 127 + 15;

    if (exponent <= 0) {
        if (exponent < -10) {
            return static_cast<uint16_t>(sign);
        }
        mantissa |= 0x00800000u;
        const uint32_t shift = static_cast<uint32_t>(14 - exponent);
        const uint32_t rounded = (mantissa + (1u << (shift - 1))) >> shift;
        return static_cast<uint16_t>(sign | rounded);
    }
    if (exponent >= 31) {
        return static_cast<uint16_t>(sign | 0x7c00u);
    }

    mantissa += 0x00001000u;
    if (mantissa & 0x00800000u) {
        mantissa = 0;
        ++exponent;
        if (exponent >= 31) {
            return static_cast<uint16_t>(sign | 0x7c00u);
        }
    }
    return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13));
}

void writeHalfFloatTexel(std::byte* destination, const glm::vec4& value) {
    const std::array<uint16_t, 4> half_values = {
        floatToHalf(value.r),
        floatToHalf(value.g),
        floatToHalf(value.b),
        floatToHalf(value.a),
    };
    std::memcpy(destination, half_values.data(), sizeof(half_values));
}

std::vector<CubemapMipPixels> createCubemapMipPixels() {
    constexpr std::array<glm::vec3, 6> face_colors = { {
        { 4.0f, 0.12f, 0.08f },
        { 0.08f, 3.2f, 3.5f },
        { 0.1f, 4.2f, 0.14f },
        { 3.3f, 0.1f, 3.0f },
        { 0.12f, 0.35f, 4.5f },
        { 4.2f, 3.1f, 0.08f },
    } };

    std::vector<CubemapMipPixels> mip_levels;
    mip_levels.reserve(cubemapMipLevels);
    uint32_t mip_size = cubemapFaceSize;
    for (uint32_t mip_level = 0; mip_level < cubemapMipLevels; ++mip_level) {
        CubemapMipPixels mip;
        mip.size = mip_size;
        for (size_t face_index = 0; face_index < mip.faces.size(); ++face_index) {
            auto& pixels = mip.faces[face_index];
            pixels.resize(static_cast<size_t>(mip_size) * mip_size * 8);
            for (uint32_t y = 0; y < mip_size; ++y) {
                for (uint32_t x = 0; x < mip_size; ++x) {
                    const uint32_t grid_size = std::max(1u, mip_size / 4);
                    const bool grid = x % grid_size == 0 || y % grid_size == 0;
                    const bool center = x == mip_size / 2 || y == mip_size / 2;
                    const float gradient = 0.55f + 0.45f * static_cast<float>(x + y) /
                                                           static_cast<float>(2 * mip_size - 2);
                    const float mip_scale = 1.0f - 0.15f * static_cast<float>(mip_level);
                    glm::vec4 value{ face_colors[face_index] * gradient * mip_scale, 1.0f };
                    if (grid) {
                        value *= 0.6f;
                        value.a = 1.0f;
                    }
                    if (center) {
                        value = glm::vec4{ 6.0f - mip_level, 6.0f - mip_level, 6.0f - mip_level,
                            1.0f };
                    }
                    const size_t offset = (static_cast<size_t>(y) * mip_size + x) * 8;
                    writeHalfFloatTexel(pixels.data() + offset, value);
                }
            }
        }
        mip_levels.push_back(std::move(mip));
        mip_size = std::max(1u, mip_size / 2);
    }
    return mip_levels;
}

renderer::TextureCubeFaces cubeFaceSpans(const std::array<std::vector<std::byte>, 6>& faces) {
    return {
        std::span<const std::byte>{ faces[0] },
        std::span<const std::byte>{ faces[1] },
        std::span<const std::byte>{ faces[2] },
        std::span<const std::byte>{ faces[3] },
        std::span<const std::byte>{ faces[4] },
        std::span<const std::byte>{ faces[5] },
    };
}

constexpr std::array depthVertices = {
    ColorVertex{ { -0.78f, -0.62f, 0.75f }, { 0.15f, 0.35f, 1.0f } },
    ColorVertex{ { 0.78f, -0.62f, 0.75f }, { 0.15f, 0.35f, 1.0f } },
    ColorVertex{ { 0.0f, 0.82f, 0.75f }, { 0.15f, 0.35f, 1.0f } },
    ColorVertex{ { -0.56f, -0.38f, 0.2f }, { 1.0f, 0.3f, 0.15f } },
    ColorVertex{ { 0.66f, -0.38f, 0.2f }, { 1.0f, 0.3f, 0.15f } },
    ColorVertex{ { 0.05f, 0.68f, 0.2f }, { 1.0f, 0.3f, 0.15f } },
};
constexpr std::array<uint16_t, 6> depthIndices = { 3, 4, 5, 0, 1, 2 };

constexpr std::array blendVertices = {
    BlendVertex{ { -0.82f, -0.58f, 0.0f }, { 0.15f, 0.35f, 1.0f, 0.62f } },
    BlendVertex{ { 0.32f, -0.58f, 0.0f }, { 0.15f, 0.35f, 1.0f, 0.62f } },
    BlendVertex{ { -0.25f, 0.68f, 0.0f }, { 0.15f, 0.35f, 1.0f, 0.62f } },
    BlendVertex{ { -0.32f, -0.68f, 0.0f }, { 0.2f, 1.0f, 0.4f, 0.58f } },
    BlendVertex{ { 0.82f, -0.68f, 0.0f }, { 0.2f, 1.0f, 0.4f, 0.58f } },
    BlendVertex{ { 0.25f, 0.58f, 0.0f }, { 0.2f, 1.0f, 0.4f, 0.58f } },
    BlendVertex{ { -0.56f, -0.15f, 0.0f }, { 1.0f, 0.25f, 0.55f, 0.56f } },
    BlendVertex{ { 0.56f, -0.15f, 0.0f }, { 1.0f, 0.25f, 0.55f, 0.56f } },
    BlendVertex{ { 0.0f, 0.82f, 0.0f }, { 1.0f, 0.25f, 0.55f, 0.56f } },
};
constexpr std::array<uint16_t, 9> blendIndices = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };

renderer::VertexBufferLayout colorVertexLayout() {
    return {
        sizeof(ColorVertex),
        {
            { 0, renderer::VertexAttributeType::Float3, offsetof(ColorVertex, position) },
            { 1, renderer::VertexAttributeType::Float3, offsetof(ColorVertex, color) },
        },
    };
}

renderer::VertexBufferLayout texturedVertexLayout() {
    return {
        sizeof(TexturedVertex),
        {
            { 0, renderer::VertexAttributeType::Float3, offsetof(TexturedVertex, position) },
            { 1, renderer::VertexAttributeType::Float2, offsetof(TexturedVertex, uv) },
        },
    };
}

renderer::VertexBufferLayout blendVertexLayout() {
    return {
        sizeof(BlendVertex),
        {
            { 0, renderer::VertexAttributeType::Float3, offsetof(BlendVertex, position) },
            { 1, renderer::VertexAttributeType::Float4, offsetof(BlendVertex, color) },
        },
    };
}

template<typename Vertex, size_t Count>
renderer::VertexBuffer createVertexBuffer(renderer::RenderDevice& device,
        const std::array<Vertex, Count>& vertices, renderer::VertexBufferLayout layout) {
    return device.createVertexBuffer(std::as_bytes(std::span{ vertices }),
            static_cast<uint32_t>(vertices.size()), std::move(layout));
}

template<size_t Count>
renderer::IndexBuffer createIndexBuffer(renderer::RenderDevice& device,
        const std::array<uint16_t, Count>& indices) {
    return device.createIndexBuffer(std::as_bytes(std::span{ indices }),
            static_cast<uint32_t>(indices.size()), renderer::IndexType::UInt16);
}

} // namespace

struct RenderSystem::Impl {
    struct Demo {
        std::string name;
        std::vector<std::unique_ptr<ShowcasePass>> passes;
    };

    void addSinglePassDemo(std::string name, std::unique_ptr<ShowcasePass> pass) {
        Demo demo;
        demo.name = std::move(name);
        demo.passes.push_back(std::move(pass));
        demos.push_back(std::move(demo));
    }

    Impl(renderer::RenderDevice& render_device, const std::filesystem::path& shader_directory,
            const std::filesystem::path& texture_path)
            : render_device(render_device) {
        addSinglePassDemo("Triangle",
                std::make_unique<TrianglePass>(
                        createVertexBuffer(render_device, triangleVertices, colorVertexLayout()),
                        createIndexBuffer(render_device, triangleIndices),
                        shader_directory / "triangle.slang"));

        const ImageData image = loadImageRGBA(texture_path);
        addSinglePassDemo("Textured Quad",
                std::make_unique<TexturedQuadPass>(createVertexBuffer(render_device,
                                                           texturedQuadVertices,
                                                           texturedVertexLayout()),
                        createIndexBuffer(render_device, quadIndices),
                        render_device.createTexture2D(image.rgba_pixels, image.width, image.height,
                                renderer::TextureFormat::RGBA8Srgb),
                        shader_directory / "textured_quad.slang"));

        const auto cubemap_pixels = createCubemapMipPixels();
        std::vector<renderer::TextureCubeMipData> cubemap_mips;
        cubemap_mips.reserve(cubemap_pixels.size());
        for (const auto& mip: cubemap_pixels) {
            cubemap_mips.push_back({ mip.size, cubeFaceSpans(mip.faces) });
        }
        addSinglePassDemo("HDR Cubemap Mips",
                std::make_unique<CubemapPass>(createVertexBuffer(render_device,
                                                      texturedQuadVertices, texturedVertexLayout()),
                        createIndexBuffer(render_device, quadIndices),
                        render_device.createTextureCube(cubemap_mips,
                                renderer::TextureFormat::RGBA16Float),
                        shader_directory / "cubemap.slang"));

        addSinglePassDemo("Render to Cubemap",
                std::make_unique<RenderToCubemapPass>(createVertexBuffer(render_device,
                                                              texturedQuadVertices,
                                                              texturedVertexLayout()),
                        createIndexBuffer(render_device, quadIndices),
                        shader_directory / "cubemap.slang"));

        addSinglePassDemo("Depth Test",
                std::make_unique<DepthTestPass>(
                        createVertexBuffer(render_device, depthVertices, colorVertexLayout()),
                        createIndexBuffer(render_device, depthIndices),
                        shader_directory / "depth_test.slang"));

        addSinglePassDemo("Alpha Blending",
                std::make_unique<AlphaBlendingPass>(
                        createVertexBuffer(render_device, blendVertices, blendVertexLayout()),
                        createIndexBuffer(render_device, blendIndices),
                        shader_directory / "alpha_blending.slang"));

        addSinglePassDemo("Push Constants",
                std::make_unique<PushConstantsPass>(
                        createVertexBuffer(render_device, triangleVertices, colorVertexLayout()),
                        createIndexBuffer(render_device, triangleIndices),
                        shader_directory / "push_constants.slang"));

        addSinglePassDemo("Uniform Buffer",
                std::make_unique<UniformBufferPass>(
                        createVertexBuffer(render_device, triangleVertices, colorVertexLayout()),
                        createIndexBuffer(render_device, triangleIndices),
                        shader_directory / "uniform_buffer.slang"));

        addSinglePassDemo("Instancing",
                std::make_unique<InstancingPass>(
                        createVertexBuffer(render_device, triangleVertices, colorVertexLayout()),
                        createIndexBuffer(render_device, triangleIndices),
                        shader_directory / "instancing.slang"));

        Demo compute_demo;
        compute_demo.name = "Compute Texture";
        auto compute_pass =
                std::make_unique<ComputeTexturePass>(shader_directory / "compute_texture.slang");
        ComputeTexturePass& compute_source = *compute_pass;
        compute_demo.passes.push_back(std::move(compute_pass));
        compute_demo.passes.push_back(std::make_unique<ImageDisplayPass>(compute_source,
                shader_directory / "image_display.slang"));
        demos.push_back(std::move(compute_demo));

        Demo offscreen_demo;
        offscreen_demo.name = "Offscreen Composite";
        auto offscreen_pass = std::make_unique<OffscreenPass>(
                createVertexBuffer(render_device, triangleVertices, colorVertexLayout()),
                createIndexBuffer(render_device, triangleIndices),
                shader_directory / "offscreen.slang");
        OffscreenPass& offscreen_source = *offscreen_pass;
        offscreen_demo.passes.push_back(std::move(offscreen_pass));
        offscreen_demo.passes.push_back(std::make_unique<ImageDisplayPass>(offscreen_source,
                shader_directory / "image_display.slang"));
        demos.push_back(std::move(offscreen_demo));
    }

    renderer::RenderDevice& render_device;
    std::vector<Demo> demos;
    size_t active_demo{ 0 };
};

RenderSystem::RenderSystem(renderer::RenderDevice& render_device,
        std::filesystem::path shader_directory, std::filesystem::path texture_path)
        : m_impl(std::make_unique<Impl>(render_device, shader_directory, texture_path)) {}

RenderSystem::~RenderSystem() = default;

bool RenderSystem::renderFrame(float elapsed_time) {
    auto& demo = m_impl->demos[m_impl->active_demo];
    std::vector<renderer::vulkan::VulkanPass*> passes;
    passes.reserve(demo.passes.size());
    for (auto& pass: demo.passes) {
        pass->setElapsedTime(elapsed_time);
        passes.push_back(pass.get());
    }
    return m_impl->render_device.renderFrame(passes);
}

void RenderSystem::nextDemo() noexcept {
    m_impl->active_demo = (m_impl->active_demo + 1) % m_impl->demos.size();
}

std::string_view RenderSystem::activeDemoName() const noexcept {
    return m_impl->demos[m_impl->active_demo].name;
}

size_t RenderSystem::demoCount() const noexcept { return m_impl->demos.size(); }

} // namespace arti::renderer_showcase
