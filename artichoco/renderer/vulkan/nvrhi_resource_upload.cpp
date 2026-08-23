#include "nvrhi_resource_upload.h"

#include <stdexcept>

namespace arti::renderer::vulkan {

nvrhi::BufferHandle createAndUploadNvrhiBuffer(nvrhi::IDevice& device,
        nvrhi::BufferDesc desc, std::span<const std::byte> data,
        nvrhi::ResourceStates final_state)
{
    if (data.empty() || desc.byteSize != data.size_bytes() ||
            final_state == nvrhi::ResourceStates::Unknown) {
        throw std::invalid_argument(
                "An NVRHI buffer upload requires matching data, size, and a final state.");
    }

    desc.enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
    nvrhi::BufferHandle buffer = device.createBuffer(desc);
    if (!buffer) {
        throw std::runtime_error("NVRHI failed to create an uploaded buffer.");
    }

    nvrhi::CommandListHandle command_list = device.createCommandList();
    if (!command_list) {
        throw std::runtime_error("NVRHI failed to create a resource upload command list.");
    }
    command_list->open();
    command_list->writeBuffer(buffer, data.data(), data.size_bytes());
    command_list->setPermanentBufferState(buffer, final_state);
    command_list->close();
    device.executeCommandList(command_list);
    return buffer;
}

nvrhi::TextureHandle createAndUploadNvrhiTexture(nvrhi::IDevice& device,
        nvrhi::TextureDesc desc, std::span<const NvrhiTextureUpload> uploads,
        nvrhi::ResourceStates final_state)
{
    if (uploads.empty() || final_state == nvrhi::ResourceStates::Unknown) {
        throw std::invalid_argument(
                "An NVRHI texture upload requires subresources and a final state.");
    }
    for (const NvrhiTextureUpload& upload : uploads) {
        if (upload.data.empty() || upload.row_pitch == 0 ||
                upload.array_slice >= desc.arraySize || upload.mip_level >= desc.mipLevels) {
            throw std::invalid_argument(
                    "An NVRHI texture upload subresource is invalid.");
        }
    }

    desc.enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
    nvrhi::TextureHandle texture = device.createTexture(desc);
    if (!texture) {
        throw std::runtime_error("NVRHI failed to create an uploaded texture.");
    }

    nvrhi::CommandListHandle command_list = device.createCommandList();
    if (!command_list) {
        throw std::runtime_error("NVRHI failed to create a texture upload command list.");
    }
    command_list->open();
    for (const NvrhiTextureUpload& upload : uploads) {
        command_list->writeTexture(texture, upload.array_slice, upload.mip_level,
                upload.data.data(), upload.row_pitch, upload.depth_pitch);
    }
    command_list->setPermanentTextureState(texture, final_state);
    command_list->close();
    device.executeCommandList(command_list);
    return texture;
}

} // namespace arti::renderer::vulkan
