#include "render_device.h"
#include "artichoco/renderer/detail/resource_owner.h"
#include "artichoco/renderer/renderer_log.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vulkan/nvrhi_shader_factory.h"
#include "artichoco/renderer/vulkan/nvrhi_resource_smoke.h"
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

    bool renderFrame(std::span<RenderPass* const> passes);
    bool renderNvrhiClearFrame(const std::array<float, 4>& clear_color);
    bool nvrhiComputeShaderSmoke(const std::filesystem::path& source_path);
    bool nvrhiResourceSmoke();
    bool supportsBindlessTextures() const noexcept;
    void requestSwapchainRecreation() noexcept;
    void waitIdle() const;

    VertexBuffer createVertexBuffer(std::span<const std::byte> data, uint32_t vertex_count,
            VertexBufferLayout layout);

    IndexBuffer createIndexBuffer(std::span<const std::byte> data, uint32_t index_count,
            IndexType index_type);

    Texture2D createTexture2D(std::span<const std::byte> texels, uint32_t width, uint32_t height,
            TextureFormat format, bool generate_mipmaps);
    TextureCube createTextureCube(std::span<const TextureCubeMipData> mip_levels,
            TextureFormat format);


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
          m_frame_manager(window, m_device, m_nvrhi_device, m_surface, info.frames_in_flight) {
    getLogChannel().info("Initialized Vulkan renderer with {} frames in flight",
            m_frame_manager.frameSlotCount());
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
        uint32_t vertex_count, VertexBufferLayout layout) {
    return detail::BufferAccess::createVertexBuffer(m_nvrhi_device.device(),
            shared_from_this(), data, vertex_count, std::move(layout));
}

IndexBuffer RenderDevice::Impl::createIndexBuffer(std::span<const std::byte> data,
        uint32_t index_count, IndexType index_type) {
    return detail::BufferAccess::createIndexBuffer(m_nvrhi_device.device(),
            shared_from_this(), data, index_count, index_type);
}

Texture2D RenderDevice::Impl::createTexture2D(std::span<const std::byte> texels, uint32_t width,
        uint32_t height, TextureFormat format, bool generate_mipmaps) {
    return detail::TextureAccess::create(m_nvrhi_device.device(),
            shared_from_this(), texels, width, height, format, generate_mipmaps);
}

TextureCube RenderDevice::Impl::createTextureCube(std::span<const TextureCubeMipData> mip_levels,
        TextureFormat format) {
    return detail::TextureAccess::createCube(m_nvrhi_device.device(),
            shared_from_this(), mip_levels, format);
}

#pragma endregion ResourceCreation

#pragma region RenderDeviceFrame
bool RenderDevice::Impl::renderFrame(std::span<RenderPass* const> passes)
{
    if (passes.empty() || std::ranges::any_of(passes,
                                  [](const RenderPass* pass) { return pass == nullptr; })) {
        throw std::invalid_argument("RenderDevice requires at least one valid NVRHI render pass.");
    }

    const auto result = m_frame_manager.renderNvrhiFrame([&](vulkan::NvrhiFrameContext& frame) {
        RenderPassPrepareContext prepare_context{
                m_nvrhi_device.device(), frame.framebuffer(), m_frame_manager.frameSlotCount()};
        for (RenderPass* pass : passes) {
            pass->prepare(prepare_context);
        }

        RenderPassContext pass_context{
                m_nvrhi_device.device(), frame.commands(), frame.framebuffer(),
                frame.colorTexture(), frame.frameSlotIndex(), frame.imageIndex(), this};
        for (RenderPass* pass : passes) {
            pass->record(pass_context);
        }
    });
    return result.rendered;
}

bool RenderDevice::Impl::renderNvrhiClearFrame(const std::array<float, 4>& clear_color)
{
    const auto result = m_frame_manager.renderNvrhiClearFrame(
            nvrhi::Color{clear_color[0], clear_color[1], clear_color[2], clear_color[3]});
    return result.rendered;
}

bool RenderDevice::Impl::nvrhiComputeShaderSmoke(const std::filesystem::path& source_path)
{
    const CompiledComputeProgram program = SlangCompiler::compileCompute({source_path});
    const auto shader_set = vulkan::createNvrhiComputeShaderSet(
            m_nvrhi_device.device(), program, "ArtiChoco NVRHI compute smoke");
    if (!shader_set.compute_shader || shader_set.binding_layouts.empty() ||
            !shader_set.binding_layouts.front()) {
        return false;
    }

    nvrhi::IDevice& device = m_nvrhi_device.device();
    nvrhi::TextureDesc source_desc;
    source_desc.setWidth(4)
            .setHeight(4)
            .setFormat(nvrhi::Format::RGBA8_UNORM)
            .setDebugName("ArtiChoco NVRHI binding smoke source")
            .enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
    nvrhi::TextureHandle source_texture = device.createTexture(source_desc);

    nvrhi::TextureDesc output_desc;
    output_desc.setWidth(4)
            .setHeight(4)
            .setFormat(nvrhi::Format::RGBA8_UNORM)
            .setDebugName("ArtiChoco NVRHI binding smoke output")
            .setIsUAV(true)
            .enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
    nvrhi::TextureHandle output_texture = device.createTexture(output_desc);
    nvrhi::TextureDesc staging_desc;
    staging_desc.setWidth(4)
            .setHeight(4)
            .setFormat(nvrhi::Format::RGBA8_UNORM)
            .setDebugName("ArtiChoco NVRHI binding smoke readback");
    nvrhi::StagingTextureHandle readback_texture =
            device.createStagingTexture(staging_desc, nvrhi::CpuAccessMode::Read);
    nvrhi::SamplerHandle sampler = device.createSampler(nvrhi::SamplerDesc{});
    if (!source_texture || !output_texture || !readback_texture || !sampler) {
        return false;
    }

    const std::array resources = {
        vulkan::NvrhiBindingResource::Texture("source_texture", *source_texture),
        vulkan::NvrhiBindingResource::Sampler("source_sampler", *sampler),
        vulkan::NvrhiBindingResource::Texture("output_texture", *output_texture),
    };
    nvrhi::BindingSetHandle binding_set = vulkan::createNvrhiBindingSet(device,
            program.reflection, 0, *shader_set.binding_layouts.front(), resources);
    if (!binding_set) {
        return false;
    }

    nvrhi::ComputePipelineDesc pipeline_desc;
    pipeline_desc.setComputeShader(shader_set.compute_shader);
    for (const nvrhi::BindingLayoutHandle& layout : shader_set.binding_layouts) {
        if (layout) {
            pipeline_desc.addBindingLayout(layout);
        }
    }
    nvrhi::ComputePipelineHandle pipeline = device.createComputePipeline(pipeline_desc);
    if (!pipeline) {
        return false;
    }

    nvrhi::CommandListHandle command_list = device.createCommandList();
    if (!command_list) {
        return false;
    }
    constexpr std::array<uint8_t, 4 * 4 * 4> source_data = {
        255, 32, 16, 255, 255, 32, 16, 255, 255, 32, 16, 255, 255, 32, 16, 255,
        255, 32, 16, 255, 255, 32, 16, 255, 255, 32, 16, 255, 255, 32, 16, 255,
        255, 32, 16, 255, 255, 32, 16, 255, 255, 32, 16, 255, 255, 32, 16, 255,
        255, 32, 16, 255, 255, 32, 16, 255, 255, 32, 16, 255, 255, 32, 16, 255,
    };
    const float time = 0.0f;
    nvrhi::ComputeState compute_state;
    compute_state.setPipeline(pipeline).addBindingSet(binding_set);
    command_list->open();
    command_list->writeTexture(source_texture, 0, 0, source_data.data(), 4 * 4);
    command_list->setComputeState(compute_state);
    command_list->setPushConstants(&time, sizeof(time));
    command_list->dispatch(1, 1, 1);
    command_list->copyTexture(readback_texture, nvrhi::TextureSlice{}, output_texture,
            nvrhi::TextureSlice{});
    command_list->close();
    device.executeCommandList(command_list);
    if (!device.waitForIdle()) {
        return false;
    }

    size_t row_pitch = 0;
    const auto* readback = static_cast<const uint8_t*>(device.mapStagingTexture(
            readback_texture, nvrhi::TextureSlice{}, nvrhi::CpuAccessMode::Read, &row_pitch));
    if (readback == nullptr || row_pitch < 4 * 4) {
        if (readback != nullptr) {
            device.unmapStagingTexture(readback_texture);
        }
        return false;
    }
    const bool output_valid = readback[0] != 0 && readback[1] != 0 &&
            readback[2] != 0 && readback[3] == 255;
    device.unmapStagingTexture(readback_texture);
    return output_valid;
}

bool RenderDevice::Impl::nvrhiResourceSmoke()
{
    return vulkan::runNvrhiResourceSmoke(m_nvrhi_device);
}

bool RenderDevice::Impl::supportsBindlessTextures() const noexcept
{
    return m_device.descriptorIndexingEnabled();
}

#pragma endregion RenderDeviceFrame
void RenderDevice::Impl::requestSwapchainRecreation() noexcept {
    m_frame_manager.requestSwapchainRecreation();
}

void RenderDevice::Impl::waitIdle() const { m_frame_manager.waitIdle(); }

RenderDevice::RenderDevice(core::Window& window,
        std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
        const RenderDeviceCreateInfo& info)
        : m_impl(std::make_shared<Impl>(window, std::move(surface_source), info)) {}

RenderDevice::~RenderDevice() { m_impl->shutdown(); }

VertexBuffer RenderDevice::createVertexBuffer(std::span<const std::byte> data,
        uint32_t vertex_count, VertexBufferLayout layout) {
    return m_impl->createVertexBuffer(data, vertex_count, std::move(layout));
}

IndexBuffer RenderDevice::createIndexBuffer(std::span<const std::byte> data, uint32_t index_count,
        IndexType index_type) {
    return m_impl->createIndexBuffer(data, index_count, index_type);
}

Texture2D RenderDevice::createTexture2D(std::span<const std::byte> texels, uint32_t width,
        uint32_t height, TextureFormat format, bool generate_mipmaps) {
    return m_impl->createTexture2D(texels, width, height, format, generate_mipmaps);
}

TextureCube RenderDevice::createTextureCube(const TextureCubeFaces& faces, uint32_t size,
        TextureFormat format) {
    const TextureCubeMipData mip{ size, faces };
    return m_impl->createTextureCube(std::span<const TextureCubeMipData>{ &mip, 1 }, format);
}

TextureCube RenderDevice::createTextureCube(std::span<const TextureCubeMipData> mip_levels,
        TextureFormat format) {
    return m_impl->createTextureCube(mip_levels, format);
}

bool RenderDevice::renderFrame(std::span<RenderPass* const> passes)
{
    return m_impl->renderFrame(passes);
}

bool RenderDevice::renderNvrhiClearFrame(const std::array<float, 4>& clear_color)
{
    return m_impl->renderNvrhiClearFrame(clear_color);
}

bool RenderDevice::nvrhiComputeShaderSmoke(const std::filesystem::path& source_path)
{
    return m_impl->nvrhiComputeShaderSmoke(source_path);
}

bool RenderDevice::nvrhiResourceSmoke()
{
    return m_impl->nvrhiResourceSmoke();
}

bool RenderDevice::supportsBindlessTextures() const noexcept
{
    return m_impl->supportsBindlessTextures();
}

void RenderDevice::requestSwapchainRecreation() noexcept { m_impl->requestSwapchainRecreation(); }

void RenderDevice::waitIdle() const { m_impl->waitIdle(); }

} // namespace arti::renderer
