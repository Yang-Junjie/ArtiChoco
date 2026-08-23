#pragma once
#include "artichoco/core/window.h"
#include "index_buffer.h"
#include "render_pass.h"
#include "texture_2d.h"
#include "texture_cube.h"
#include "vertex_buffer.h"

#include <cstddef>
#include <cstdint>

#include <array>
#include <filesystem>
#include <memory>
#include <span>
#include <string>

namespace arti::renderer::vulkan {
class VulkanSurfaceSource;
} // namespace arti::renderer::vulkan

namespace arti::renderer {

struct RenderDeviceCreateInfo {
    std::string application_name{ "ArtiChoco" };
    uint32_t frames_in_flight{ 2 };
#if defined(NDEBUG)
    bool enable_validation{ false };
#else
    bool enable_validation{ true };
#endif
};

class RenderDevice {
public:
    RenderDevice(core::Window& window, std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
            const RenderDeviceCreateInfo& info = {});
    ~RenderDevice();

    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;

    VertexBuffer createVertexBuffer(std::span<const std::byte> data, uint32_t vertex_count,
            VertexBufferLayout layout);

    IndexBuffer createIndexBuffer(std::span<const std::byte> data, uint32_t index_count,
            IndexType index_type = IndexType::UInt32);

    Texture2D createTexture2D(std::span<const std::byte> texels, uint32_t width, uint32_t height,
            TextureFormat format = TextureFormat::RGBA8Srgb, bool generate_mipmaps = true);

    TextureCube createTextureCube(const TextureCubeFaces& faces, uint32_t size,
            TextureFormat format = TextureFormat::RGBA8Srgb);
    TextureCube createTextureCube(std::span<const TextureCubeMipData> mip_levels,
            TextureFormat format = TextureFormat::RGBA8Srgb);

    bool renderFrame(std::span<RenderPass* const> passes);
    bool renderNvrhiClearFrame(const std::array<float, 4>& clear_color);
    bool nvrhiComputeShaderSmoke(const std::filesystem::path& source_path);
    bool nvrhiResourceSmoke();
    bool supportsBindlessTextures() const noexcept;
    void requestSwapchainRecreation() noexcept;
    void waitIdle() const;

private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;
};

} // namespace arti::renderer
