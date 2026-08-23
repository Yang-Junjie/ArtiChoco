#pragma once

#include "artichoco/renderer/render_pass.h"

#include <filesystem>
#include <memory>

namespace arti::basic_lighting {

class LightingPass;

class PresentPass final : public renderer::RenderPass {
public:
    PresentPass(LightingPass& source, std::filesystem::path shader_path);
    ~PresentPass() override;

    void prepare(renderer::RenderPassPrepareContext& context) override;
    void record(renderer::RenderPassContext& context) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::basic_lighting
