#pragma once
#include "artichoco/core/window.h"
#include "index_buffer.h"
#include "texture_2d.h"
#include "vertex_buffer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>

namespace arti::renderer::vulkan {
class VulkanSurfaceSource;
}

namespace arti::renderer {

struct RendererCreateInfo {
    std::string application_name{"ArtiChoco"};
    std::filesystem::path shader_path;
    std::string vertex_entry_point{"vertexMain"};
    std::string fragment_entry_point{"fragmentMain"};
    uint32_t frames_in_flight{2};
    std::array<float, 4> clear_color{0.04f, 0.08f, 0.12f, 1.0f};
#if defined(NDEBUG)
    bool enable_validation{false};
#else
    bool enable_validation{true};
#endif
};

struct DrawCommand {
    const VertexBuffer* vertex_buffer{nullptr};
    const IndexBuffer* index_buffer{nullptr};
    const Texture2D* base_color_texture{nullptr};
    uint32_t index_count{0};
    uint32_t first_index{0};
    int32_t vertex_offset{0};
    std::array<float, 16> transform{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
};

class Renderer {
public:
    Renderer(
        core::Window& window,
        std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
        const RendererCreateInfo& info = {});
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    VertexBuffer createVertexBuffer(
        std::span<const std::byte> data,
        uint32_t vertex_count,
        VertexBufferLayout layout);
    IndexBuffer createIndexBuffer(
        std::span<const std::byte> data,
        uint32_t index_count,
        IndexType index_type = IndexType::UInt32);
    Texture2D createTexture2D(
        std::span<const std::byte> rgba_pixels,
        uint32_t width,
        uint32_t height,
        TextureFormat format = TextureFormat::RGBA8Srgb);

    bool renderFrame(std::span<const DrawCommand> draw_commands = {});
    void requestSwapchainRecreation() noexcept;
    void setClearColor(const std::array<float, 4>& color) noexcept;
    void waitIdle() const;

private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;
};

} // namespace arti::renderer
