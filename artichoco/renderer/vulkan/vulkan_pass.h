#pragma once
#include "artichoco/renderer/render_frame_data.h"

namespace arti::renderer::vulkan {

class VulkanPassContext;
class VulkanPassPrepareContext;

class VulkanPass {
public:
    virtual ~VulkanPass() = default;

    virtual void prepare(VulkanPassPrepareContext& context) {}

    virtual void record(VulkanPassContext& context) = 0;

    virtual void applyFrameData(const RenderFrameData& frame_data) {}
};

} // namespace arti::renderer::vulkan
