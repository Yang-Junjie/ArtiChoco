#include "nvrhi_vulkan_device.h"

#include "artichoco/renderer/renderer_log.h"
#include "nvrhi_vulkan_dispatch.h"
#include "vulkan_context.h"
#include "vulkan_device.h"

#include <nvrhi/validation.h>
#include <nvrhi/vulkan.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace arti::renderer::vulkan {
namespace {

class NvrhiMessageCallback final : public nvrhi::IMessageCallback {
public:
    void message(nvrhi::MessageSeverity severity, const char* message_text) override
    {
        const char* text = message_text != nullptr ? message_text : "NVRHI emitted an empty message.";
        try {
            switch (severity) {
                case nvrhi::MessageSeverity::Info:
                    getLogChannel().info("NVRHI: {}", text);
                    break;
                case nvrhi::MessageSeverity::Warning:
                    getLogChannel().warn("NVRHI: {}", text);
                    break;
                case nvrhi::MessageSeverity::Error:
                    getLogChannel().error("NVRHI: {}", text);
                    break;
                case nvrhi::MessageSeverity::Fatal:
                    getLogChannel().fatal("NVRHI: {}", text);
                    break;
            }
        } catch (...) {
        }
    }
};

std::vector<const char*> instanceExtensionNames(const VulkanContext& context)
{
    std::vector<const char*> names;
    names.reserve(context.enabledExtensions().size());
    for (const std::string& extension : context.enabledExtensions()) {
        names.push_back(extension.c_str());
    }
    return names;
}

} // namespace

struct NvrhiVulkanDevice::Impl {
    Impl(const VulkanContext& context, const VulkanDevice& device, bool enable_validation)
    {
        std::vector<const char*> instance_extensions = instanceExtensionNames(context);
        std::vector<const char*> device_extensions{
            device.enabledExtensions().begin(), device.enabledExtensions().end()};

        nvrhi::vulkan::DeviceDesc desc{};
        desc.errorCB = &message_callback;
        desc.instance = static_cast<VkInstance>(*context.instance());
        desc.physicalDevice = static_cast<VkPhysicalDevice>(*device.physicalDevice());
        desc.device = static_cast<VkDevice>(*device.device());
        desc.graphicsQueue = static_cast<VkQueue>(*device.graphicsQueue());
        desc.graphicsQueueIndex = static_cast<int>(device.graphicsQueueFamily());
        desc.instanceExtensions = instance_extensions.data();
        desc.numInstanceExtensions = instance_extensions.size();
        desc.deviceExtensions = device_extensions.data();
        desc.numDeviceExtensions = device_extensions.size();

        detail::initializeNvrhiVulkanDispatcher(desc.instance, desc.device);
        native_device = nvrhi::vulkan::createDevice(desc);
        if (native_device.Get() == nullptr) {
            throw std::runtime_error("Failed to create the NVRHI Vulkan device.");
        }

        exposed_device = native_device;
        if (enable_validation) {
            exposed_device = nvrhi::validation::createValidationLayer(native_device.Get());
            if (exposed_device.Get() == nullptr) {
                throw std::runtime_error("Failed to create the NVRHI validation layer.");
            }
        }

        getLogChannel().info(
                "Created NVRHI Vulkan device{}", enable_validation ? " with validation" : "");
    }

    NvrhiMessageCallback message_callback;
    nvrhi::vulkan::DeviceHandle native_device;
    nvrhi::DeviceHandle exposed_device;
};

NvrhiVulkanDevice::NvrhiVulkanDevice(
        const VulkanContext& context, const VulkanDevice& device, bool enable_validation)
    : m_impl(std::make_unique<Impl>(context, device, enable_validation))
{}

NvrhiVulkanDevice::~NvrhiVulkanDevice() = default;

nvrhi::IDevice& NvrhiVulkanDevice::device() const noexcept
{
    return *m_impl->exposed_device.Get();
}

nvrhi::vulkan::IDevice& NvrhiVulkanDevice::nativeDevice() const noexcept
{
    return *m_impl->native_device.Get();
}

} // namespace arti::renderer::vulkan
