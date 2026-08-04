#pragma once
#include "artichoco/core/window.h"
#include "index_buffer.h"
#include "texture_2d.h"
#include "vertex_buffer.h"

#include <cstddef>
#include <cstdint>

#include <memory>
#include <span>
#include <string>

namespace arti::renderer::vulkan {
class VulkanPass;
class VulkanSurfaceSource;
} // namespace arti::renderer::vulkan

namespace arti::renderer {

struct RendererCreateInfo {
    std::string application_name{"ArtiChoco"};
    uint32_t frames_in_flight{2};
#if defined(NDEBUG)
    bool enable_validation{false};
#else
    bool enable_validation{true};
#endif
};

class Renderer {
public:
    Renderer(core::Window& window,
             std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
             const RendererCreateInfo& info = {});
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    VertexBuffer createVertexBuffer(std::span<const std::byte> data, uint32_t vertex_count, VertexBufferLayout layout);
    IndexBuffer createIndexBuffer(std::span<const std::byte> data,
                                  uint32_t index_count,
                                  IndexType index_type = IndexType::UInt32);
    Texture2D createTexture2D(std::span<const std::byte> rgba_pixels,
                              uint32_t width,
                              uint32_t height,
                              TextureFormat format = TextureFormat::RGBA8Srgb);

    bool renderFrame(std::span<vulkan::VulkanPass* const> passes);
    void requestSwapchainRecreation() noexcept;
    void waitIdle() const;

private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;
};

} // namespace arti::renderer
