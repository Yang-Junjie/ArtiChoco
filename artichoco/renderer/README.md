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
