# Renderer Architecture

`RenderDevice` is the public facade. Its implementation owns the Vulkan bootstrap
and the NVRHI Vulkan device in this order:

1. Surface source, Vulkan instance, surface, physical device, and queues.
2. `nvrhi::vulkan::IDevice`, wrapping the existing Vulkan handles.
3. `VulkanSwapchain`, which wraps swapchain images as NVRHI textures and
   framebuffers.
4. `VulkanFrameManager`, which records and submits NVRHI command lists.

Application passes implement the backend-independent `RenderPass` interface.
`RenderDevice::renderFrame` gives each pass an NVRHI device, command list,
framebuffer, and swapchain color texture. Resource creation, pipelines, bindings,
uploads, and state transitions use NVRHI APIs; business renderer headers do not
expose Vulkan handles or the removed legacy Vulkan pass API.

## Vulkan boundary

The native Vulkan boundary is limited to bootstrap and presentation:

1. SDL provides a `VkSurfaceKHR` source.
2. `VulkanContext` creates the instance and validation messenger.
3. `VulkanDevice` selects the physical device and graphics/present queues.
4. `VulkanSwapchain` creates image views and exposes the native swapchain for
   acquire and present.
5. `VulkanFrameManager` acquires an image with a native semaphore, records NVRHI
   commands, bridges NVRHI queue wait/signal calls, and presents with the native
   present queue.

NVRHI owns resource state tracking and command-list lifetime tracking. The frame
   manager calls `runGarbageCollection` after submissions and after idle waits;
   NVRHI keeps referenced resources alive until the GPU has completed each command
   list. Resize and out-of-date recovery invalidate NVRHI back-buffer wrappers,
   wait for the device, and recreate the swapchain before the next frame.

## Shaders and reflection

Slang remains the source and reflection authority. `SlangCompiler` targets SPIR-V.
`nvrhi_shader_factory` creates NVRHI vertex, fragment, and compute shaders from
that bytecode and maps Slang reflection data to NVRHI binding layouts and sets:

- descriptor sets become NVRHI `registerSpace` values;
- constant buffers, sampled/storage textures, structured/raw buffers, and samplers
  map to their NVRHI binding types;
- arrays retain their reflected descriptor counts;
- push-constant ranges are represented by NVRHI push-constant bindings.

## Coordinate convention

Rendering code follows NVRHI's Direct3D-style screen convention on every backend.
The NVRHI Vulkan backend applies a negative viewport height, so GLM projection
matrices configured with `GLM_FORCE_DEPTH_ZERO_TO_ONE` must be used without an
additional `projection[1][1]` sign flip. Fullscreen triangles map top-left UVs to
clip space with `(uv.x * 2 - 1, 1 - uv.y * 2)`. Mesh geometry uses outward-facing
counter-clockwise winding and declares `frontCounterClockwise` explicitly in its
raster state.

## Resources and state

`VertexBuffer`, `IndexBuffer`, `Texture2D`, and `TextureCube` store NVRHI handles.
Uploads use NVRHI command lists and automatic state tracking. Texture mip chains
are generated on the CPU when requested and uploaded as explicit NVRHI subresources;
GPU synchronization and final resource states remain NVRHI-managed.

Public resources hold a small `ResourceOwner` shared pointer. It keeps the
`RenderDevice` implementation alive when a resource outlives the facade and is
used only to reject cross-device resource access. NVRHI command-list lifetime
tracking, `waitForIdle`, and `runGarbageCollection` provide GPU-safe destruction;
there is no renderer-side deferred-release queue.

## Frame and threading model

The renderer uses one render thread. Constructing or destroying `RenderDevice`,
creating resources, recording passes, requesting swapchain recreation, and calling
`waitIdle` must happen on that thread. Worker threads may perform file IO,
decompression, image decoding, and other CPU-only work, then hand completed data
to the render thread for upload.

`VulkanFrameManager` keeps a frames-in-flight ring. Each slot has an image-acquired
semaphore, an NVRHI event query, and a submitted flag. A slot is reused only after
its query completes, while the swapchain image index is selected independently by
Vulkan acquire.

## Verification

`examples/test_app` provides the retained renderer smoke coverage:

- `--smoke-nvrhi`: resource upload/release, GPU mipmap readback, Slang
  reflection, cubemap-face attachment readback, clear/present, and resize;
- `--smoke-render`: compute, MRT graphics, exception recovery, resize, and scene/
  project integration.
- `vulkan_frame_policy_smoke`: deterministic minimize, resize, out-of-date, and
  suboptimal swapchain policy coverage.

The old Vulkan-only showcase and hello-triangle implementations were removed.
The replacement `examples/graphic_examples` programs all use the Application +
Layer + `RenderPass` structure, NVRHI resources and commands, and Slang SPIR-V
compilation/reflection:

- `hello_triangle`: the minimal NVRHI graphics pipeline;
- `hello_cube`: indexed geometry, push constants, an sRGB texture, GPU mipmaps,
  and reflected texture/sampler bindings;
- `basic_lighting`: normal-based directional and Blinn-Phong lighting, `D32`
  depth, and an HDR offscreen/present pass pair;
- `instancing`: 225 animated cubes in one indexed draw with per-instance vertex
  data and per-back-buffer depth attachments;
- `render_to_texture`: a procedural 512 by 512 render target sampled on a
  depth-tested cube in a second pass.
