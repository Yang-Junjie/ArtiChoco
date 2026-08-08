#pragma once

namespace arti::renderer::vulkan {
class VulkanImage;
}

namespace arti::renderer_showcase {

class SampledImageSource {
public:
    virtual ~SampledImageSource() = default;

    virtual const renderer::vulkan::VulkanImage& output() const = 0;
};

} // namespace arti::renderer_showcase
