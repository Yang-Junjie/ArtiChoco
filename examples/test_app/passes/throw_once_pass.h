#pragma once
#include "artichoco/renderer/vulkan/vulkan_pass.h"

namespace arti::test_app {

class ThrowOncePass final : public renderer::vulkan::VulkanPass {
public:
    void record(renderer::vulkan::VulkanPassContext& context) override;
    bool didThrow() const noexcept;

private:
    bool m_did_throw{false};
};

} // namespace arti::test_app
