#pragma once
#include "artichoco/core/window.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
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

class Renderer {
public:
    Renderer(
        core::Window& window,
        std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
        const RendererCreateInfo& info = {});
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool renderFrame();
    void requestSwapchainRecreation() noexcept;
    void setClearColor(const std::array<float, 4>& color) noexcept;
    void waitIdle() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::renderer
