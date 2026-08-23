#pragma once
#include "artichoco/renderer/render_pass.h"

namespace arti::test_app {

class ThrowOncePass final : public renderer::RenderPass {
public:
    void record(renderer::RenderPassContext& context) override;
    bool didThrow() const noexcept;

private:
    bool m_did_throw{false};
};

} // namespace arti::test_app
