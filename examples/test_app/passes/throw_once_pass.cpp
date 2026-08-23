#include "throw_once_pass.h"

#include <stdexcept>

namespace arti::test_app {

void ThrowOncePass::record(renderer::RenderPassContext& context)
{
    if (m_did_throw) {
        return;
    }

    context.commands().clearTextureFloat(&context.colorTexture(), nvrhi::AllSubresources,
            nvrhi::Color{0.1f, 0.0f, 0.0f, 1.0f});

    m_did_throw = true;
    throw std::runtime_error("Intentional NVRHI frame recording failure.");
}

bool ThrowOncePass::didThrow() const noexcept
{
    return m_did_throw;
}

} // namespace arti::test_app
