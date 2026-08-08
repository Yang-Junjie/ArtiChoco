#pragma once
#include "artichoco/renderer/vulkan/vulkan_pass.h"

namespace arti::renderer_showcase {

class ShowcasePass : public renderer::vulkan::VulkanPass {
public:
    ~ShowcasePass() override = default;

    virtual void setElapsedTime(float elapsed_time) noexcept
    {
        (void)elapsed_time;
    }
};

} // namespace arti::renderer_showcase
