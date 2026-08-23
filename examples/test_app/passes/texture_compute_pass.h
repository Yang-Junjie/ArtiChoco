#pragma once
#include "artichoco/renderer/render_pass.h"
#include "frame_data.h"

#include <filesystem>
#include <memory>

namespace arti::renderer {
class Texture2D;
} // namespace arti::renderer

namespace arti::test_app {

class TextureComputePass final : public renderer::RenderPass {
public:
    TextureComputePass(std::shared_ptr<renderer::Texture2D> source, const std::filesystem::path& shader_path);
    ~TextureComputePass() override;

    void applyFrameData(const RenderFrameData& frame_data);
    void prepare(renderer::RenderPassPrepareContext& context) override;
    void record(renderer::RenderPassContext& context) override;

    nvrhi::ITexture& output() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::test_app
