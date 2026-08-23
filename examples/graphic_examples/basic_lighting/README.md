# Basic Lighting

This example renders three rotating cubes with per-face normals, directional
diffuse lighting, Blinn-Phong highlights, and depth testing.

The geometry pass renders into NVRHI `RGBA16_FLOAT` color and `D32` depth
attachments. A second pass samples the HDR color texture and presents it to the
Vulkan swapchain. Both passes use runtime Slang SPIR-V compilation and reflection.

Build the repository, then run:

```text
build/bin/basic_lighting.exe
```

The smoke mode also resizes the window to verify attachment recreation:

```text
build/bin/basic_lighting.exe --smoke
```
