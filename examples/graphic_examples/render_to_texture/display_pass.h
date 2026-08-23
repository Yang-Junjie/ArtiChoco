#pragma once

#include "artichoco/renderer/render_pass.h"

#include <filesystem>
#include <memory>

namespace arti::renderer {
class RenderDevice;
}

namespace arti::render_to_texture {

class RenderTexturePass;

class DisplayPass final : public renderer::RenderPass {
public:
    DisplayPass(renderer::RenderDevice& device, RenderTexturePass& source,
            std::filesystem::path shader_path);
    ~DisplayPass() override;

    void setRotation(float radians) noexcept;
    void prepare(renderer::RenderPassPrepareContext& context) override;
    void record(renderer::RenderPassContext& context) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::render_to_texture
