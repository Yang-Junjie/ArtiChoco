#include "render_device.h"
#include "artichoco/renderer/detail/resource_owner.h"
#include "artichoco/renderer/renderer_log.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vulkan/nvrhi_resource_smoke.h"
#include "artichoco/renderer/vulkan/nvrhi_shader_factory.h"
#include "artichoco/renderer/vulkan/nvrhi_vulkan_device.h"
#include "artichoco/renderer/vulkan/vulkan_context.h"
#include "artichoco/renderer/vulkan/vulkan_device.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_surface.h"
#include "artichoco/renderer/vulkan/vulkan_surface_source.h"
#include "detail/buffer_access.h"
#include "detail/texture_access.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace arti::renderer {
#pragma region HelperFunctions
namespace {

// ensure the VulkanSurfaceSource is not null and not transfer ownership.
vulkan::VulkanSurfaceSource& requireSurfaceSource(
        const std::unique_ptr<vulkan::VulkanSurfaceSource>& source) {
    if (!source) {
        throw std::invalid_argument("RenderDevice requires a surface source.");
    }
    return *source;
}

// Create a VulkanContextCreateInfo from RenderDeviceCreateInfo
vulkan::VulkanContextCreateInfo makeContextCreateInfo(const RenderDeviceCreateInfo& info) {
    vulkan::VulkanContextCreateInfo context_info;
    context_info.application_name = info.application_name;
    context_info.enable_validation = info.enable_validation;
    return context_info;
}

} // namespace
#pragma endregion HelperFunctions

#pragma region RenderDeviceImpl
struct RenderDevice::Impl final : detail::ResourceOwner,
                                  std::enable_shared_from_this<RenderDevice::Impl> {
    Impl(core::Window& window, std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
            const RenderDeviceCreateInfo& info);
    ~Impl();

    void shutdown();

    RenderFrameResult renderFrame(std::span<RenderPass* const> passes);
    bool renderNvrhiClearFrame(const std::array<float, 4>& clear_color);
    bool nvrhiComputeShaderSmoke(const std::filesystem::path& source_path);
    bool nvrhiResourceSmoke();
    bool supportsBindlessTextures() const noexcept;
    RenderFormatSupport queryFormatSupport(RenderDeviceFormat format) const noexcept;
    RenderSwapchainInfo swapchainInfo() const noexcept;
    void requestSwapchainRecreation() noexcept;
    void setVsync(bool enabled) noexcept;
    bool vsync() const noexcept;
    void waitIdle() const;

    VertexBuffer createVertexBuffer(std::span<const std::byte> data, uint32_t vertex_count,
            VertexBufferLayout layout, std::string_view debug_name);

    IndexBuffer createIndexBuffer(std::span<const std::byte> data, uint32_t index_count,
            IndexType index_type, std::string_view debug_name);

    Texture2D createTexture2D(std::span<const std::byte> texels, uint32_t width, uint32_t height,
            TextureFormat format, bool generate_mipmaps, std::string_view debug_name);
    TextureCube createTextureCube(std::span<const TextureCubeMipData> mip_levels,
            TextureFormat format, std::string_view debug_name);


private:
    std::unique_ptr<vulkan::VulkanSurfaceSource> m_surface_source;
    vulkan::VulkanContext m_context;
    vulkan::VulkanSurface m_surface;
    vulkan::VulkanDevice m_device;
    vulkan::NvrhiVulkanDevice m_nvrhi_device;
    vulkan::VulkanFrameManager m_frame_manager;
    bool m_shutdown{ false };
};

#pragma endregion RenderDeviceImpl
RenderDevice::Impl::Impl(core::Window& window,
        std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
        const RenderDeviceCreateInfo& info)
        : m_surface_source(std::move(surface_source)),
          m_context(makeContextCreateInfo(info), requireSurfaceSource(m_surface_source)),
          m_surface(m_context.instance(), requireSurfaceSource(m_surface_source)),
          m_device(m_context.instance(), m_surface.handle()),
          m_nvrhi_device(m_context, m_device, info.enable_validation),
          m_frame_manager(window, m_device, m_nvrhi_device, m_surface, info.frames_in_flight,
                  info.vsync) {
    getLogChannel().info("Initialized Vulkan renderer with {} frames in flight (vsync {})",
            m_frame_manager.frameSlotCount(), info.vsync);
}

RenderDevice::Impl::~Impl() {
    try {
        shutdown();
    } catch (...) {
    }
}

void RenderDevice::Impl::shutdown() {
    if (m_shutdown) {
        return;
    }
    waitIdle();
    m_shutdown = true;
}

#pragma region ResourceCreation
VertexBuffer RenderDevice::Impl::createVertexBuffer(std::span<const std::byte> data,
        uint32_t vertex_count, VertexBufferLayout layout, std::string_view debug_name) {
    return detail::BufferAccess::createVertexBuffer(m_nvrhi_device.device(), shared_from_this(),
            data, vertex_count, std::move(layout), debug_name);
}

IndexBuffer RenderDevice::Impl::createIndexBuffer(std::span<const std::byte> data,
        uint32_t index_count, IndexType index_type, std::string_view debug_name) {
    return detail::BufferAccess::createIndexBuffer(m_nvrhi_device.device(), shared_from_this(),
            data, index_count, index_type, debug_name);
}

Texture2D RenderDevice::Impl::createTexture2D(std::span<const std::byte> texels, uint32_t width,
        uint32_t height, TextureFormat format, bool generate_mipmaps, std::string_view debug_name) {
    return detail::TextureAccess::create(m_nvrhi_device.device(), shared_from_this(), texels, width,
            height, format, generate_mipmaps, debug_name);
}

TextureCube RenderDevice::Impl::createTextureCube(std::span<const TextureCubeMipData> mip_levels,
        TextureFormat format, std::string_view debug_name) {
    return detail::TextureAccess::createCube(m_nvrhi_device.device(), shared_from_this(),
            mip_levels, format, debug_name);
}

#pragma endregion ResourceCreation

#pragma region RenderDeviceFrame
RenderFrameResult RenderDevice::Impl::renderFrame(std::span<RenderPass* const> passes) {
    if (passes.empty() ||
            std::ranges::any_of(passes, [](const RenderPass* pass) { return pass == nullptr; })) {
        throw std::invalid_argument("RenderDevice requires at least one valid NVRHI render pass.");
    }

    const auto result = m_frame_manager.renderNvrhiFrame([&](vulkan::NvrhiFrameContext& frame) {
        RenderPassPrepareContext prepare_context{ m_nvrhi_device.device(), frame.framebuffer(),
            m_frame_manager.frameSlotCount() };
        for (RenderPass* pass: passes) {
            pass->prepare(prepare_context);
        }

        RenderPassContext pass_context{ m_nvrhi_device.device(), frame.commands(),
            frame.framebuffer(), frame.colorTexture(), frame.frameSlotIndex(), frame.imageIndex(),
            this };
        for (RenderPass* pass: passes) {
            pass->record(pass_context);
        }
    });
    return { result.completed_frame_slot, result.submitted_frame_slot, result.rendered };
}

bool RenderDevice::Impl::renderNvrhiClearFrame(const std::array<float, 4>& clear_color) {
    const auto result = m_frame_manager.renderNvrhiClearFrame(
            nvrhi::Color{ clear_color[0], clear_color[1], clear_color[2], clear_color[3] });
    return result.rendered;
}


bool RenderDevice::Impl::supportsBindlessTextures() const noexcept {
    return m_device.descriptorIndexingEnabled();
}

RenderFormatSupport RenderDevice::Impl::queryFormatSupport(
        RenderDeviceFormat format) const noexcept {
    nvrhi::Format nvrhi_format = nvrhi::Format::UNKNOWN;
    vk::Format vulkan_format = vk::Format::eUndefined;
    bool depth = false;
    switch (format) {
        case RenderDeviceFormat::RGBA8Unorm:
            nvrhi_format = nvrhi::Format::RGBA8_UNORM;
            vulkan_format = vk::Format::eR8G8B8A8Unorm;
            break;
        case RenderDeviceFormat::RGBA8Srgb:
            nvrhi_format = nvrhi::Format::SRGBA8_UNORM;
            vulkan_format = vk::Format::eR8G8B8A8Srgb;
            break;
        case RenderDeviceFormat::RGBA16Float:
            nvrhi_format = nvrhi::Format::RGBA16_FLOAT;
            vulkan_format = vk::Format::eR16G16B16A16Sfloat;
            break;
        case RenderDeviceFormat::D32Float:
            nvrhi_format = nvrhi::Format::D32;
            vulkan_format = vk::Format::eD32Sfloat;
            depth = true;
            break;
    }

    const nvrhi::FormatSupport support = m_nvrhi_device.device().queryFormatSupport(nvrhi_format);
    const auto has = [support](nvrhi::FormatSupport flag) noexcept {
        return (support & flag) != nvrhi::FormatSupport::None;
    };
    RenderFormatSupport result;
    result.texture = has(nvrhi::FormatSupport::Texture);
    result.shader_sample = has(nvrhi::FormatSupport::ShaderSample);
    result.render_target = has(nvrhi::FormatSupport::RenderTarget);
    result.depth_stencil = has(nvrhi::FormatSupport::DepthStencil);

    if ((depth && !result.depth_stencil) || (!depth && !result.render_target)) {
        return result;
    }
    try {
        const vk::ImageUsageFlags usage = depth ? vk::ImageUsageFlagBits::eDepthStencilAttachment
                                                : vk::ImageUsageFlagBits::eColorAttachment;
        const vk::ImageFormatProperties properties =
                m_device.physicalDevice().getImageFormatProperties(vulkan_format,
                        vk::ImageType::e2D, vk::ImageTiling::eOptimal, usage, {});
        result.sample_count_mask = static_cast<uint32_t>(properties.sampleCounts);
    } catch (...) {
        result.sample_count_mask = 0;
    }
    return result;
}

SwapchainPresentMode toSwapchainPresentMode(vk::PresentModeKHR mode) noexcept {
    switch (mode) {
        case vk::PresentModeKHR::eMailbox:
            return SwapchainPresentMode::Mailbox;
        case vk::PresentModeKHR::eImmediate:
            return SwapchainPresentMode::Immediate;
        default:
            return SwapchainPresentMode::Fifo;
    }
}

RenderSwapchainInfo RenderDevice::Impl::swapchainInfo() const noexcept {
    return {
        m_frame_manager.swapchainWidth(),
        m_frame_manager.swapchainHeight(),
        m_frame_manager.swapchainIsRenderable(),
        toSwapchainPresentMode(m_frame_manager.presentMode()),
        m_frame_manager.vsync(),
    };
}

#pragma endregion RenderDeviceFrame
void RenderDevice::Impl::requestSwapchainRecreation() noexcept {
    m_frame_manager.requestSwapchainRecreation();
}

void RenderDevice::Impl::setVsync(bool enabled) noexcept { m_frame_manager.setVsync(enabled); }

bool RenderDevice::Impl::vsync() const noexcept { return m_frame_manager.vsync(); }

void RenderDevice::Impl::waitIdle() const { m_frame_manager.waitIdle(); }

RenderDevice::RenderDevice(core::Window& window,
        std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
        const RenderDeviceCreateInfo& info)
        : m_impl(std::make_shared<Impl>(window, std::move(surface_source), info)) {}

RenderDevice::~RenderDevice() { m_impl->shutdown(); }

VertexBuffer RenderDevice::createVertexBuffer(std::span<const std::byte> data,
        uint32_t vertex_count, VertexBufferLayout layout, std::string_view debug_name) {
    return m_impl->createVertexBuffer(data, vertex_count, std::move(layout), debug_name);
}

IndexBuffer RenderDevice::createIndexBuffer(std::span<const std::byte> data, uint32_t index_count,
        IndexType index_type, std::string_view debug_name) {
    return m_impl->createIndexBuffer(data, index_count, index_type, debug_name);
}

Texture2D RenderDevice::createTexture2D(std::span<const std::byte> texels, uint32_t width,
        uint32_t height, TextureFormat format, bool generate_mipmaps, std::string_view debug_name) {
    return m_impl->createTexture2D(texels, width, height, format, generate_mipmaps, debug_name);
}

TextureCube RenderDevice::createTextureCube(const TextureCubeFaces& faces, uint32_t size,
        TextureFormat format, std::string_view debug_name) {
    const TextureCubeMipData mip{ size, faces };
    return m_impl->createTextureCube(std::span<const TextureCubeMipData>{ &mip, 1 }, format,
            debug_name);
}

TextureCube RenderDevice::createTextureCube(std::span<const TextureCubeMipData> mip_levels,
        TextureFormat format, std::string_view debug_name) {
    return m_impl->createTextureCube(mip_levels, format, debug_name);
}

RenderFrameResult RenderDevice::renderFrame(std::span<RenderPass* const> passes) {
    return m_impl->renderFrame(passes);
}

bool RenderDevice::renderNvrhiClearFrame(const std::array<float, 4>& clear_color) {
    return m_impl->renderNvrhiClearFrame(clear_color);
}

bool RenderDevice::supportsBindlessTextures() const noexcept {
    return m_impl->supportsBindlessTextures();
}

RenderFormatSupport RenderDevice::queryFormatSupport(RenderDeviceFormat format) const noexcept {
    return m_impl->queryFormatSupport(format);
}

RenderSwapchainInfo RenderDevice::swapchainInfo() const noexcept { return m_impl->swapchainInfo(); }

void RenderDevice::requestSwapchainRecreation() noexcept { m_impl->requestSwapchainRecreation(); }

void RenderDevice::setVsync(bool enabled) noexcept { m_impl->setVsync(enabled); }

bool RenderDevice::vsync() const noexcept { return m_impl->vsync(); }

void RenderDevice::waitIdle() const { m_impl->waitIdle(); }

} // namespace arti::renderer
