#pragma once

#include "artichoco/renderer/render_pass.h"

#include <filesystem>
#include <memory>

namespace arti::render_to_texture {

class RenderTexturePass final : public renderer::RenderPass {
public:
    explicit RenderTexturePass(std::filesystem::path shader_path);
    ~RenderTexturePass() override;

    void setTime(float seconds) noexcept;
    void prepare(renderer::RenderPassPrepareContext& context) override;
    void record(renderer::RenderPassContext& context) override;

    nvrhi::ITexture& output() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::render_to_texture
