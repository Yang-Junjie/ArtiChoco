#include "vulkan_context.h"

#include "artichoco/renderer/renderer_log.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace arti::renderer::vulkan {
namespace {

bool containsExtension(const std::vector<vk::ExtensionProperties>& available, std::string_view name)
{
    return std::ranges::any_of(available, [name](const vk::ExtensionProperties& extension) {
        return name == extension.extensionName;
    });
}

void appendUnique(std::vector<std::string>& extensions, std::string_view extension)
{
    if (std::ranges::none_of(extensions, [extension](const std::string& value) { return value == extension; })) {
        extensions.emplace_back(extension);
    }
}

bool hasLayer(const std::vector<vk::LayerProperties>& available, std::string_view name)
{
    return std::ranges::any_of(available, [name](const vk::LayerProperties& layer) {
        return name == layer.layerName;
    });
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT,
    const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
    void*)
{
    const char* message = callback_data != nullptr && callback_data->pMessage != nullptr
        ? callback_data->pMessage
        : "Vulkan validation message contained no text.";
    try {
        if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
            getLogChannel().error("Validation: {}", message);
        } else if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
            getLogChannel().warn("Validation: {}", message);
        } else if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
            getLogChannel().debug("Validation: {}", message);
        } else {
            getLogChannel().trace("Validation: {}", message);
        }
    } catch (...) {
    }
    return VK_FALSE;
}

} // namespace

VulkanContext::VulkanContext(const VulkanContextCreateInfo& info, const VulkanSurfaceSource& surface_source)
{
    auto extensions = surface_source.requiredInstanceExtensions();
    const auto available_extensions = m_context.enumerateInstanceExtensionProperties();
    const uint32_t loader_api_version = m_context.enumerateInstanceVersion();
    if (info.api_version > loader_api_version) {
        throw std::runtime_error("The Vulkan loader does not support the requested API version.");
    }
    m_api_version = info.api_version;

    bool validation_enabled = info.enable_validation;
    if (validation_enabled &&
        !hasLayer(m_context.enumerateInstanceLayerProperties(), "VK_LAYER_KHRONOS_validation")) {
        getLogChannel().warn("VK_LAYER_KHRONOS_validation is unavailable; validation is disabled");
        validation_enabled = false;
    }
    if (validation_enabled && !containsExtension(available_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        getLogChannel().warn("VK_EXT_debug_utils is unavailable; validation is disabled");
        validation_enabled = false;
    }
    if (validation_enabled) {
        appendUnique(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    vk::InstanceCreateFlags instance_flags{};
#if defined(__APPLE__)
    if (containsExtension(available_extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        appendUnique(extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        instance_flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
    }
#endif

    for (const auto& extension : extensions) {
        if (!containsExtension(available_extensions, extension)) {
            throw std::runtime_error("Required Vulkan instance extension is unavailable: " + extension);
        }
    }

    std::vector<const char*> extension_names;
    extension_names.reserve(extensions.size());
    for (const auto& extension : extensions) {
        extension_names.push_back(extension.c_str());
    }

    vk::ApplicationInfo application_info{};
    application_info.setPApplicationName(info.application_name.c_str())
        .setApplicationVersion(info.application_version)
        .setPEngineName("ArtiChoco")
        .setEngineVersion(VK_MAKE_API_VERSION(0, 0, 1, 0))
        .setApiVersion(m_api_version);

    vk::DebugUtilsMessengerCreateInfoEXT debug_info{};
    debug_info
        .setMessageSeverity(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
        .setMessageType(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
        .setPfnUserCallback(debugCallback);

    const std::vector<const char*> validation_layers = {"VK_LAYER_KHRONOS_validation"};

    vk::InstanceCreateInfo instance_info{};
    instance_info.setFlags(instance_flags)
        .setPApplicationInfo(&application_info)
        .setPEnabledExtensionNames(extension_names);
    if (validation_enabled) {
        instance_info.setPEnabledLayerNames(validation_layers).setPNext(&debug_info);
    }

    m_instance = vk::raii::Instance{m_context, instance_info};
    if (validation_enabled) {
        m_debug_messenger = vk::raii::DebugUtilsMessengerEXT{m_instance, debug_info};
    }
    getLogChannel().info(
        "Created Vulkan {}.{}.{} instance for '{}'{}",
        VK_API_VERSION_MAJOR(m_api_version),
        VK_API_VERSION_MINOR(m_api_version),
        VK_API_VERSION_PATCH(m_api_version),
        info.application_name,
        validation_enabled ? " with validation" : "");
}

const vk::raii::Context& VulkanContext::context() const noexcept
{
    return m_context;
}

const vk::raii::Instance& VulkanContext::instance() const noexcept
{
    return m_instance;
}

uint32_t VulkanContext::apiVersion() const noexcept
{
    return m_api_version;
}

} // namespace arti::renderer::vulkan
