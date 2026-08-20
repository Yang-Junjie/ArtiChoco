#pragma once

#include <filesystem>
#include <memory>

namespace arti::renderer {
class RenderDevice;
} // namespace arti::renderer

namespace arti::hello_triangle {

class TriangleRenderer {
public:
    TriangleRenderer(renderer::RenderDevice& render_device, std::filesystem::path shader_path);
    ~TriangleRenderer();

    TriangleRenderer(const TriangleRenderer&) = delete;
    TriangleRenderer& operator=(const TriangleRenderer&) = delete;

    bool renderFrame();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::hello_triangle
