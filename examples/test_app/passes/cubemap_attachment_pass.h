#pragma once

#include "artichoco/renderer/render_pass.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace arti::test_app {

class CubemapAttachmentPass final : public renderer::RenderPass {
public:
    explicit CubemapAttachmentPass(std::filesystem::path shader_path);
    ~CubemapAttachmentPass() override;

    void prepare(renderer::RenderPassPrepareContext& context) override;
    void record(renderer::RenderPassContext& context) override;

    bool verifyReadback();
    std::array<uint8_t, 4> readbackPixel() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::test_app
