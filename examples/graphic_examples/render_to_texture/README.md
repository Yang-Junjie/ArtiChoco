# Render To Texture

This example uses two ordered NVRHI render passes. `RenderTexturePass` draws an
animated procedural image into a fixed 512 by 512 `RGBA8_UNORM` render target.
`DisplayPass` samples that texture on a rotating, depth-tested cube and renders it
to the Vulkan swapchain.

The example exercises automatic render-target to shader-resource transitions,
Slang texture/sampler and push-constant reflection, offscreen framebuffer
ownership, and swapchain resize handling.

Build the repository, then run:

```text
build/bin/render_to_texture.exe
```

Use `--smoke` to render several frames and validate the resize path automatically.
