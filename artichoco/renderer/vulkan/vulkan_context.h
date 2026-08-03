#pragma once
#include "vulkan_surface_source.h"

#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <string>

namespace arti::renderer::vulkan {

struct VulkanContextCreateInfo {
    std::string application_name{"ArtiChoco"};
    uint32_t application_version{VK_MAKE_API_VERSION(0, 0, 1, 0)};
    uint32_t api_version{VK_API_VERSION_1_4};
#if defined(NDEBUG)
    bool enable_validation{false};
#else
    bool enable_validation{true};
#endif
};

class VulkanContext {
public:
    VulkanContext(const VulkanContextCreateInfo& info, const VulkanSurfaceSource& surface_source);

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    const vk::raii::Context& context() const noexcept;
    const vk::raii::Instance& instance() const noexcept;
    uint32_t apiVersion() const noexcept;

private:
    vk::raii::Context m_context;
    vk::raii::Instance m_instance{nullptr};
    vk::raii::DebugUtilsMessengerEXT m_debug_messenger{nullptr};
    uint32_t m_api_version{VK_API_VERSION_1_0};
};

} // namespace arti::renderer::vulkan
