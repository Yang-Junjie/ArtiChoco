#pragma once
#include "vulkan_presentation_info.h"

#include <cstddef>

#include <vulkan/vulkan.hpp>

namespace arti::renderer {
class IndexBuffer;
class Texture2D;
class TextureCube;
class VertexBuffer;

namespace detail {
class DeferredResourceOwner;
}

namespace vulkan {

class VulkanAllocator;
class VulkanCommandRecorder;
class VulkanContext;
class VulkanDescriptorAllocator;
class VulkanDevice;
class VulkanFrameContext;
class VulkanImage;
class VulkanPipelineCache;
class VulkanUploadContext;

class VulkanPassPrepareContext {
public:
    VulkanPassPrepareContext(const VulkanContext& vulkan_context,
                             const VulkanDevice& device,
                             VulkanAllocator& allocator,
                             VulkanUploadContext& upload_context,
                             VulkanDescriptorAllocator& descriptor_allocator,
                             VulkanPipelineCache& pipeline_cache,
                             size_t frame_slot_count,
                             VulkanPresentationInfo presentation_info) noexcept;

    VulkanPassPrepareContext(const VulkanPassPrepareContext&) = delete;
    VulkanPassPrepareContext& operator=(const VulkanPassPrepareContext&) = delete;

    const VulkanContext& vulkanContext() const noexcept;
    const VulkanDevice& device() const noexcept;
    VulkanAllocator& allocator() const noexcept;
    VulkanUploadContext& uploadContext() const noexcept;
    VulkanDescriptorAllocator& descriptorAllocator() const noexcept;
    VulkanPipelineCache& pipelineCache() const noexcept;
    size_t frameSlotCount() const noexcept;
    VulkanPresentationInfo presentationInfo() const noexcept;

private:
    const VulkanContext& m_vulkan_context;
    const VulkanDevice& m_device;
    VulkanAllocator& m_allocator;
    VulkanUploadContext& m_upload_context;
    VulkanDescriptorAllocator& m_descriptor_allocator;
    VulkanPipelineCache& m_pipeline_cache;
    size_t m_frame_slot_count{0};
    VulkanPresentationInfo m_presentation_info;
};

class VulkanPassContext {
public:
    VulkanPassContext(VulkanFrameContext& frame,
                      const detail::DeferredResourceOwner* resource_owner,
                      VulkanPipelineCache& pipeline_cache) noexcept;

    VulkanPassContext(const VulkanPassContext&) = delete;
    VulkanPassContext& operator=(const VulkanPassContext&) = delete;

    VulkanFrameContext& frame() const noexcept;
    VulkanCommandRecorder& commands() const noexcept;
    VulkanPipelineCache& pipelineCache() const noexcept;
    vk::Buffer buffer(const VertexBuffer& vertex_buffer) const;
    vk::Buffer buffer(const IndexBuffer& index_buffer) const;
    const VulkanImage& image(const Texture2D& texture) const;
    const VulkanImage& image(const TextureCube& texture) const;

private:
    VulkanFrameContext& m_frame;
    const detail::DeferredResourceOwner* m_resource_owner{nullptr};
    VulkanPipelineCache& m_pipeline_cache;
};

} // namespace vulkan
} // namespace arti::renderer
