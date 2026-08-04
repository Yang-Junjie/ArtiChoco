#pragma once

namespace arti::renderer::vulkan {

class VulkanPassContext;
class VulkanPassPrepareContext;

class VulkanPass {
public:
    virtual ~VulkanPass() = default;

    virtual void prepare(VulkanPassPrepareContext& context) {}

    virtual void record(VulkanPassContext& context) = 0;
};

} // namespace arti::renderer::vulkan
