#pragma once

#include "artichoco/renderer/render_pass.h"
#include "artichoco/renderer/texture_2d.h"

#include <filesystem>
#include <memory>

namespace arti::renderer {
class RenderDevice;
}

namespace arti::hello_cube {

class CubePass final : public renderer::RenderPass {
public:
    CubePass(renderer::RenderDevice& device, renderer::Texture2D texture,
            std::filesystem::path shader_path);
    ~CubePass() override;

    void setRotation(float radians) noexcept;
    void prepare(renderer::RenderPassPrepareContext& context) override;
    void record(renderer::RenderPassContext& context) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::hello_cube
