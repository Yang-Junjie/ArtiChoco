#pragma once

#include "artichoco/renderer/render_pass.h"

#include <filesystem>
#include <memory>

namespace arti::renderer {
class RenderDevice;
}

namespace arti::basic_lighting {

class LightingPass final : public renderer::RenderPass {
public:
    LightingPass(renderer::RenderDevice& device, std::filesystem::path shader_path);
    ~LightingPass() override;

    void setRotation(float radians) noexcept;
    void prepare(renderer::RenderPassPrepareContext& context) override;
    void record(renderer::RenderPassContext& context) override;

    nvrhi::ITexture& colorOutput() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::basic_lighting
