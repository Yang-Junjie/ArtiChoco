# Renderer Architecture

`Renderer` is the public facade. Its `Impl` is a composition root whose members
make Vulkan ownership and destruction order explicit:

1. Surface source, instance, surface, device, allocator, and upload services.
2. Descriptor allocation and `VulkanFrameManager`.
3. `DeferredReleaseQueue` for resources referenced by submitted frame slots.

`Impl` coordinates resource creation and one `renderFrame` call. It is not a
backend, RHI, render graph, or catch-all Vulkan abstraction. Vulkan behavior stays
in the focused objects under `renderer/vulkan`.

`DeferredResourceOwner` is a lifetime anchor, not the release queue itself.
Public resource handles retain a shared owner so the allocator and device outlive
those handles even when the public `Renderer` has already been destroyed. The
owner delegates release bookkeeping to `DeferredReleaseQueue`.

## Frame terminology

- A frame slot is one entry in the frames-in-flight ring. `VulkanFrameSlot` owns
  its command pool, command buffer, acquire semaphore, and in-flight fence.
- A swapchain image is selected independently by `imageIndex()` and is not a
  frame slot.
- `VulkanFrameToken` owns one acquired swapchain image from command recording
  through submit, present, or abandonment.
- A completed frame slot has a signaled fence and may retire deferred resources.

The frame sequence is:

1. Prepare every pass before acquiring a swapchain image.
2. Begin a frame and retire the previously completed frame slot, if any.
3. Record the ordered passes into the token's frame slot.
4. Submit and present the token.
5. Mark that frame slot as submitted for deferred resource release.

If preparation throws, no frame has been acquired. If recording or submission
throws, destruction of the active token abandons the recording, replaces its frame
slot synchronization objects, and invalidates the acquired swapchain. The next
frame recreates the swapchain before recording.

## Threading model

The renderer currently uses one render thread. The following operations must run
on that thread:

- Constructing and destroying `Renderer`.
- Calling `renderFrame`, `waitIdle`, or requesting swapchain recreation.
- Creating vertex buffers, index buffers, and textures.
- Allocating descriptors and recording uploads or Vulkan commands.

Worker threads may perform file IO, decompression, image decoding, mesh parsing,
and other CPU-only asset work. A future asynchronous asset system should send
completed CPU data to the render thread through an upload queue. Worker threads
must not call Vulkan or resource-creation methods directly.

`VertexBuffer`, `IndexBuffer`, and `Texture2D` may be released on a worker thread
after no CPU-side render state can reference them. During normal rendering, their
destructors enqueue a move-only task through `DeferredResourceOwner`. The render
thread executes it after every submitted frame slot that could reference the
resource has completed.

The queue mutex protects only deferred-release bookkeeping. Vulkan destruction is
performed outside that lock. It does not make `Renderer`, pass recording, uploads,
or individual resource objects generally thread-safe.

Renderer shutdown must happen on the render thread after producers stop creating
new upload requests. Shutdown waits for the device and drains the queue. A resource
handle released after shutdown destroys its allocation synchronously on the
releasing thread; its shared owner keeps the idle allocator and device alive until
that destruction finishes.

## Vulkan pass lifetime

Application rendering is an explicit ordered list of `VulkanPass` objects.
Prepare and record contexts are non-owning, non-copyable views valid only for their
respective calls. Passes must not retain either context.

Passes explicitly declare synchronization with `vk::PipelineStageFlags2`,
`vk::AccessFlags2`, and `vk::ImageLayout`. There is no automatic state tracking or
render graph. A producing pass records the barrier required by its next consumer.
The final swapchain writer transitions the image to
`vk::ImageLayout::ePresentSrcKHR` before submission.

`VulkanBufferState` and `VulkanImageState` are explicit values used to construct
native Synchronization2 barriers. They do not store or infer a resource's current
state. Upload calls require a final state and record the dependency from their Copy
write to that caller-selected state.

## Vulkan buffer semantics

`VulkanBuffer` owns one VMA allocation while leaving Vulkan usage and synchronization
visible to its caller. It has two memory policies:

- `DeviceLocal` prefers device memory and adds `eTransferDst` so
  `uploadInitial()` can populate a newly-created buffer through the shared staging
  upload context. The caller supplies the first consumer's `VulkanBufferState`.
- `HostVisible` prefers host memory, stays mapped for its lifetime, and exposes
  bounded `write()` calls. Each write flushes the written allocation range.

Neither policy tracks GPU state. `uploadInitial()` establishes only the dependency
from its Copy write to the supplied first-consumer state. Rewriting a buffer after
GPU use requires the caller to synchronize the old use with the new transfer or
host write before invoking the update.

`VulkanBindingSet::writeUniformBuffer()` and `writeStorageBuffer()` validate both
the reflected descriptor type and the buffer's Vulkan usage. Dynamic per-frame
data uses one Host-visible Uniform Buffer per frame slot. A slot is written only
after `beginFrame()` has waited for that slot's fence; persistent mapping alone is
not a CPU/GPU synchronization mechanism.

## Graphics pipelines and attachments

`VulkanGraphicsPipelineCreateInfo` describes the attachment contract used by
dynamic rendering. Its ordered `color_formats` list defines the Fragment output
locations, and `color_blend_attachments` optionally supplies one native Vulkan
blend state per color attachment. An empty blend-state list means opaque writes
to every color component. A non-empty list must match the format count exactly.

Devices enable `independentBlend` when it is supported. MRT remains available
without that optional feature, but pipelines on such devices must use identical
blend states for every color attachment. Differing states are rejected during
pipeline creation rather than deferred to Vulkan Validation.

Depth is optional and explicit. Enabling depth testing or writes without a depth
format is invalid. A zero-stride, attribute-free `VertexBufferLayout` represents
no vertex inputs and can be used with `VulkanCommandRecorder::draw()` for
procedural full-screen geometry.

`VulkanImage` can own sampled color attachments by combining native
`eColorAttachment` and `eSampled` usage. It does not remember attachment state.
The MRT producer transitions every output from its known prior state to attachment
write, then to the state required by the next sampling pass.

Pipeline objects, binding layouts, binding sets, and pass-owned images must outlive
every submitted frame slot that references them. For now, destroy or replace those
objects only after `Renderer::waitIdle()`. Connecting pass-owned Vulkan objects to
the deferred-release queue is a later resource-lifetime phase.
