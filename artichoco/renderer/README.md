# Renderer Threading Model

The renderer currently uses one render thread. The following operations must run
on that thread:

- Constructing and destroying `Renderer`.
- Calling `renderFrame`, `waitIdle`, or requesting swapchain recreation.
- Creating vertex buffers, index buffers, and textures.
- Recording uploads and Vulkan commands.

Worker threads may perform file IO, decompression, image decoding, mesh parsing,
and other CPU-only asset work. A future asynchronous asset system should send
completed CPU data to the render thread through an upload queue. Worker threads
must not call Vulkan or resource-creation methods directly.

`VertexBuffer`, `IndexBuffer`, and `Texture2D` may be released on a worker thread
after no CPU-side draw command can reference them. Their destructors only enqueue
a move-only release task through `DeferredResourceOwner`. The render thread
executes that task after every frame fence which could reference the resource has
completed. This protects GPU lifetime without calling `waitIdle` for each asset.

The release mutex protects only the deferred-release queue and its per-frame
bookkeeping. It does not make `Renderer`, draw submission, uploads, or individual
resource objects generally thread-safe.

Renderer shutdown must happen on the render thread after producers stop creating
new upload requests. Shutdown waits for the device, drains pending releases, and
then tears down Vulkan objects.

## Vulkan pass recording

`Renderer` owns Vulkan initialization, resource uploads, frame synchronization,
swapchain acquisition, submission, and presentation. Application rendering is
expressed as an ordered list of `VulkanPass` objects. Before acquiring a frame,
the renderer calls each pass with a `VulkanPassPrepareContext` so shader
compilation and persistent resource creation cannot poison an active frame. It
then calls each pass with a `VulkanPassContext` to record the current frame.

Prepare and record contexts are non-owning, non-copyable views valid only for the
duration of their respective calls. Passes must not store either context. A pass
may keep the Vulkan objects it creates and narrow references, such as the logical
device used by those objects, provided the pass is destroyed before its renderer.

Passes explicitly declare Vulkan synchronization with `vk::PipelineStageFlags2`,
`vk::AccessFlags2`, and `vk::ImageLayout`. There is currently no automatic state
tracking or render graph. A pass which produces a resource must record the barrier
required by the next consumer. The final pass that writes the swapchain image must
transition it to `vk::ImageLayout::ePresentSrcKHR` before the frame is submitted.

Pipeline objects, binding layouts, binding sets, and pass-owned images must outlive
every frame that references them. Destroy or replace those objects only after the
renderer is idle, or through a future deferred-release path designed for pass-owned
Vulkan resources.
