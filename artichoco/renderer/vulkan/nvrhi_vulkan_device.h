#pragma once

#include <memory>

namespace nvrhi {
class IDevice;

namespace vulkan {
class IDevice;
}
} // namespace nvrhi

namespace arti::renderer::vulkan {

class VulkanContext;
class VulkanDevice;

class NvrhiVulkanDevice {
public:
    NvrhiVulkanDevice(
            const VulkanContext& context, const VulkanDevice& device, bool enable_validation);
    ~NvrhiVulkanDevice();

    NvrhiVulkanDevice(const NvrhiVulkanDevice&) = delete;
    NvrhiVulkanDevice& operator=(const NvrhiVulkanDevice&) = delete;

    nvrhi::IDevice& device() const noexcept;
    nvrhi::vulkan::IDevice& nativeDevice() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::renderer::vulkan
