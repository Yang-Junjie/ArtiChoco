#pragma once
#include <filesystem>
#include <memory>
#include <string_view>

namespace arti::renderer {
class RenderDevice;
} // namespace arti::renderer

namespace arti::renderer_showcase {
class RenderSystem {
public:
    RenderSystem(renderer::RenderDevice& render_device,
                 std::filesystem::path shader_directory,
                 std::filesystem::path texture_path);
    ~RenderSystem();

    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;

    bool renderFrame(float elapsed_time);
    void nextDemo() noexcept;
    std::string_view activeDemoName() const noexcept;
    size_t demoCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
} // namespace arti::renderer_showcase
