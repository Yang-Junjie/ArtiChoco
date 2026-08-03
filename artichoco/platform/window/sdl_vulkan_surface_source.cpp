#include "sdl_vulkan_surface_source.h"

#include "artichoco/platform/platform_log.h"
#include "sdl_window.h"

#include <SDL3/SDL_vulkan.h>

#include <stdexcept>

namespace arti::platform {
namespace {

class SDLVulkanSurfaceSource final : public renderer::vulkan::VulkanSurfaceSource {
public:
    explicit SDLVulkanSurfaceSource(SDLWindow& window)
        : m_window(window)
    {}

    std::vector<std::string> requiredInstanceExtensions() const override
    {
        Uint32 count = 0;
        const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);
        if (extensions == nullptr) {
            throw std::runtime_error(std::string{"Failed to query SDL Vulkan instance extensions: "} + SDL_GetError());
        }

        std::vector<std::string> result;
        result.reserve(count);
        for (Uint32 index = 0; index < count; ++index) {
            result.emplace_back(extensions[index]);
        }
        return result;
    }

    VkResult createSurface(
        VkInstance instance,
        const VkAllocationCallbacks* allocator,
        VkSurfaceKHR* surface) const override
    {
        if (m_window.nativeHandle() == nullptr || surface == nullptr) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        if (!SDL_Vulkan_CreateSurface(m_window.nativeHandle(), instance, allocator, surface)) {
            getLogChannel().error("Failed to create SDL Vulkan surface: {}", SDL_GetError());
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        return VK_SUCCESS;
    }

private:
    SDLWindow& m_window;
};

} // namespace

std::unique_ptr<renderer::vulkan::VulkanSurfaceSource> createSDLVulkanSurfaceSource(core::Window& window)
{
    auto* sdl_window = dynamic_cast<SDLWindow*>(&window);
    if (sdl_window == nullptr) {
        throw std::invalid_argument("The Vulkan surface source requires an SDL window.");
    }
    return std::make_unique<SDLVulkanSurfaceSource>(*sdl_window);
}

} // namespace arti::platform
