#pragma once

#include "artichoco/renderer/render_pass.h"

#include <filesystem>
#include <memory>

namespace arti::renderer {
class RenderDevice;
}

namespace arti::hello_triangle {

class TrianglePass final : public renderer::RenderPass {
public:
    TrianglePass(renderer::RenderDevice& device, std::filesystem::path shader_path);
    ~TrianglePass() override;

    void prepare(renderer::RenderPassPrepareContext& context) override;
    void record(renderer::RenderPassContext& context) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::hello_triangle
