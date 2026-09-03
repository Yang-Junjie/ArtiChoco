#pragma once
#include "artichoco/core/window.h"
#include "index_buffer.h"
#include "render_device_capabilities.h"
#include "render_pass.h"
#include "texture_2d.h"
#include "texture_cube.h"
#include "vertex_buffer.h"

#include <cstddef>
#include <cstdint>

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace arti::renderer::vulkan {
class VulkanSurfaceSource;
} // namespace arti::renderer::vulkan

namespace arti::renderer {

// Vulkan present mode as seen by the rest of the engine. FIFO is vsync; MAILBOX is
// vsync with triple-buffering; IMMEDIATE is uncapped and may tear.
enum class SwapchainPresentMode : uint8_t {
    Fifo = 0,
    Mailbox,
    Immediate,
};

inline const char* toString(SwapchainPresentMode mode) noexcept {
    switch (mode) {
        case SwapchainPresentMode::Mailbox:
            return "MAILBOX";
        case SwapchainPresentMode::Immediate:
            return "IMMEDIATE";
        case SwapchainPresentMode::Fifo:
            return "FIFO";
    }
    return "FIFO";
}

struct RenderDeviceCreateInfo {
    std::string application_name{ "ArtiChoco" };
    uint32_t frames_in_flight{ 2 };
#if defined(NDEBUG)
    bool enable_validation{ false };
#else
    bool enable_validation{ true };
#endif
    // On: MAILBOX if the surface supports it, otherwise FIFO.
    // Off: IMMEDIATE if supported, otherwise MAILBOX, otherwise FIFO.
    bool vsync{ true };
};

struct RenderSwapchainInfo {
    uint32_t width{ 0 };
    uint32_t height{ 0 };
    bool available{ false };
    SwapchainPresentMode present_mode{ SwapchainPresentMode::Fifo };
    bool vsync{ true };
};

struct RenderFrameResult {
    std::optional<size_t> completed_frame_slot;
    std::optional<size_t> submitted_frame_slot;
    bool rendered{ false };

    bool wasRendered() const noexcept { return rendered; }
};

class RenderDevice {
public:
    RenderDevice(core::Window& window, std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
            const RenderDeviceCreateInfo& info = {});
    ~RenderDevice();

    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;

    VertexBuffer createVertexBuffer(std::span<const std::byte> data, uint32_t vertex_count,
            VertexBufferLayout layout, std::string_view debug_name = {});

    IndexBuffer createIndexBuffer(std::span<const std::byte> data, uint32_t index_count,
            IndexType index_type = IndexType::UInt32, std::string_view debug_name = {});

    Texture2D createTexture2D(std::span<const std::byte> texels, uint32_t width, uint32_t height,
            TextureFormat format = TextureFormat::RGBA8Srgb, bool generate_mipmaps = true,
            std::string_view debug_name = {});

    TextureCube createTextureCube(const TextureCubeFaces& faces, uint32_t size,
            TextureFormat format = TextureFormat::RGBA8Srgb, std::string_view debug_name = {});
    TextureCube createTextureCube(std::span<const TextureCubeMipData> mip_levels,
            TextureFormat format = TextureFormat::RGBA8Srgb, std::string_view debug_name = {});

    RenderFrameResult renderFrame(std::span<RenderPass* const> passes);
    bool renderNvrhiClearFrame(const std::array<float, 4>& clear_color);
    bool supportsBindlessTextures() const noexcept;
    RenderFormatSupport queryFormatSupport(RenderDeviceFormat format) const noexcept;
    RenderSwapchainInfo swapchainInfo() const noexcept;
    void requestSwapchainRecreation() noexcept;
    void setVsync(bool enabled) noexcept;
    bool vsync() const noexcept;
    void waitIdle() const;

private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;
};

} // namespace arti::renderer
