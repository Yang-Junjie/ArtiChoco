#pragma once

namespace arti::renderer::vulkan {

class NvrhiVulkanDevice;

// Creates, uploads, transitions, executes, and releases a small set of NVRHI resources.
// This is intentionally isolated from the legacy Vulkan resource wrappers while migration
// is in progress.
bool runNvrhiResourceSmoke(NvrhiVulkanDevice& nvrhi_device);

} // namespace arti::renderer::vulkan
