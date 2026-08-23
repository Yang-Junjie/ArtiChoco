#pragma once

#include "artichoco/renderer/render_pass.h"

#include <filesystem>
#include <memory>

namespace arti::renderer {
class RenderDevice;
}

namespace arti::instancing {

class InstancingPass final : public renderer::RenderPass {
public:
    InstancingPass(renderer::RenderDevice& device, std::filesystem::path shader_path);
    ~InstancingPass() override;

    void setTime(float seconds) noexcept;
    void prepare(renderer::RenderPassPrepareContext& context) override;
    void record(renderer::RenderPassContext& context) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::instancing
