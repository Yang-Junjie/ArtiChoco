#include "nvrhi_resource_smoke.h"

#include "artichoco/renderer/renderer_log.h"
#include "nvrhi_mipmap.h"
#include "nvrhi_vulkan_device.h"

#include <nvrhi/nvrhi.h>

#include <array>
#include <cstdint>
#include <exception>

namespace arti::renderer::vulkan {

bool runNvrhiResourceSmoke(NvrhiVulkanDevice& nvrhi_device)
{
    try {
        nvrhi::IDevice& device = nvrhi_device.device();

        constexpr std::array<uint32_t, 16> vertex_data = {
            0x00000000u, 0x3f800000u, 0x00000000u, 0x3f800000u,
            0x3f800000u, 0x00000000u, 0x3f800000u, 0x00000000u,
            0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u,
            0x3f800000u, 0x3f800000u, 0x00000000u, 0x00000000u,
        };
        constexpr std::array<uint16_t, 6> index_data = {0, 1, 2, 2, 3, 0};
        constexpr std::array<uint8_t, 4 * 4 * 4> texture_data = {
            255, 0, 0, 255,     0, 255, 0, 255,     0, 0, 255, 255,     255, 255, 255, 255,
            255, 255, 0, 255,   0, 255, 255, 255,   255, 0, 255, 255,   32, 32, 32, 255,
            128, 0, 0, 255,     0, 128, 0, 255,     0, 0, 128, 255,     128, 128, 128, 255,
            64, 64, 64, 255,    192, 192, 192, 255, 16, 16, 16, 255,  240, 240, 240, 255,
        };

        nvrhi::BufferDesc vertex_desc;
        vertex_desc.setByteSize(sizeof(vertex_data))
            .setDebugName("ArtiChoco NVRHI smoke vertex buffer")
            .setIsVertexBuffer(true)
            .enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
        nvrhi::BufferHandle vertex_buffer = device.createBuffer(vertex_desc);
        if (!vertex_buffer) {
            getLogChannel().error("NVRHI resource smoke failed to create a vertex buffer.");
            return false;
        }

        nvrhi::BufferDesc index_desc;
        index_desc.setByteSize(sizeof(index_data))
            .setDebugName("ArtiChoco NVRHI smoke index buffer")
            .setIsIndexBuffer(true)
            .enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
        nvrhi::BufferHandle index_buffer = device.createBuffer(index_desc);
        if (!index_buffer) {
            getLogChannel().error("NVRHI resource smoke failed to create an index buffer.");
            return false;
        }

        nvrhi::TextureDesc texture_desc;
        texture_desc.setWidth(4)
            .setHeight(4)
            .setFormat(nvrhi::Format::RGBA8_UNORM)
            .setDebugName("ArtiChoco NVRHI smoke texture")
            .setIsUAV(true)
            .enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
        nvrhi::TextureHandle texture = device.createTexture(texture_desc);
        if (!texture) {
            getLogChannel().error("NVRHI resource smoke failed to create a texture.");
            return false;
        }

        nvrhi::CommandListHandle command_list = device.createCommandList();
        if (!command_list) {
            getLogChannel().error("NVRHI resource smoke failed to create a command list.");
            return false;
        }

        command_list->open();
        command_list->writeBuffer(vertex_buffer, vertex_data.data(), sizeof(vertex_data));
        command_list->writeBuffer(index_buffer, index_data.data(), sizeof(index_data));
        command_list->writeTexture(texture, 0, 0, texture_data.data(), 4 * 4);
        command_list->setBufferState(vertex_buffer, nvrhi::ResourceStates::VertexBuffer);
        command_list->setBufferState(index_buffer, nvrhi::ResourceStates::IndexBuffer);
        command_list->setTextureState(texture, nvrhi::TextureSubresourceSet{},
                nvrhi::ResourceStates::UnorderedAccess);
        command_list->close();

        device.executeCommandList(command_list);
        if (!device.waitForIdle()) {
            getLogChannel().error("NVRHI resource smoke waitForIdle failed.");
            return false;
        }
        device.runGarbageCollection();

        constexpr std::array<uint8_t, 4 * 4 * 4> mip_source = [] {
            std::array<uint8_t, 4 * 4 * 4> result{};
            for (size_t pixel = 0; pixel < 16; ++pixel) {
                result[pixel * 4 + 0] = 48;
                result[pixel * 4 + 1] = 96;
                result[pixel * 4 + 2] = 192;
                result[pixel * 4 + 3] = 255;
            }
            return result;
        }();
        nvrhi::TextureDesc mip_desc;
        mip_desc.setWidth(4)
                .setHeight(4)
                .setMipLevels(3)
                .setFormat(nvrhi::Format::RGBA8_UNORM)
                .setDebugName("ArtiChoco NVRHI mipmap smoke")
                .setIsUAV(true)
                .setIsTypeless(true)
                .enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
        nvrhi::TextureHandle mip_texture = device.createTexture(mip_desc);
        if (!mip_texture) {
            getLogChannel().error("NVRHI mipmap smoke failed to create a texture.");
            return false;
        }
        uploadAndGenerateNvrhiTextureMipmaps(device, mip_texture,
                std::as_bytes(std::span{mip_source}), 4 * 4,
                nvrhi::Format::RGBA8_UNORM, nvrhi::Format::RGBA8_UNORM,
                nvrhi::ResourceStates::CopySource);

        nvrhi::TextureDesc mip_staging_desc;
        mip_staging_desc.setWidth(4)
                .setHeight(4)
                .setMipLevels(3)
                .setFormat(nvrhi::Format::RGBA8_UNORM)
                .setDebugName("ArtiChoco NVRHI mipmap readback");
        nvrhi::StagingTextureHandle mip_readback =
                device.createStagingTexture(mip_staging_desc, nvrhi::CpuAccessMode::Read);
        nvrhi::CommandListHandle readback_commands = device.createCommandList();
        if (!mip_readback || !readback_commands) {
            getLogChannel().error("NVRHI mipmap smoke failed to create readback resources.");
            return false;
        }
        const nvrhi::TextureSlice final_mip = nvrhi::TextureSlice().setMipLevel(2);
        readback_commands->open();
        readback_commands->copyTexture(mip_readback, final_mip, mip_texture, final_mip);
        readback_commands->close();
        device.executeCommandList(readback_commands);
        if (!device.waitForIdle()) {
            getLogChannel().error("NVRHI mipmap smoke waitForIdle failed.");
            return false;
        }
        size_t mip_row_pitch = 0;
        const auto* final_pixel = static_cast<const uint8_t*>(device.mapStagingTexture(
                mip_readback, final_mip, nvrhi::CpuAccessMode::Read, &mip_row_pitch));
        const bool mip_valid = final_pixel != nullptr && mip_row_pitch >= 4 &&
                final_pixel[0] == 48 && final_pixel[1] == 96 &&
                final_pixel[2] == 192 && final_pixel[3] == 255;
        if (final_pixel != nullptr) {
            device.unmapStagingTexture(mip_readback);
        }
        if (!mip_valid) {
            getLogChannel().error("NVRHI compute mipmap readback did not match the source color.");
            return false;
        }
        device.runGarbageCollection();
        getLogChannel().info("NVRHI resource creation/upload/release smoke passed");
        return true;
    } catch (const std::exception& error) {
        getLogChannel().error("NVRHI resource smoke failed: {}", error.what());
        return false;
    }
}

} // namespace arti::renderer::vulkan
