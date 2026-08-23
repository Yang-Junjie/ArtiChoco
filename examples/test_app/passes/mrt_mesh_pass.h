#pragma once
#include "artichoco/renderer/render_pass.h"
#include "frame_data.h"
#include "texture_compute_pass.h"

#include <array>
#include <filesystem>
#include <memory>

namespace arti::test_app {

class MrtMeshPass final : public renderer::RenderPass {
public:
    MrtMeshPass(TextureComputePass& texture_source, const std::filesystem::path& shader_path);
    ~MrtMeshPass() override;

    void applyFrameData(const RenderFrameData& frame_data);
    void setClearColor(const std::array<float, 4>& color) noexcept;
    void prepare(renderer::RenderPassPrepareContext& context) override;
    void record(renderer::RenderPassContext& context) override;

    nvrhi::ITexture& colorOutput() const;
    nvrhi::ITexture& auxiliaryOutput() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::test_app
