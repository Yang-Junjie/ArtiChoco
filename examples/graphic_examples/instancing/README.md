# Instancing

This example renders a `kGridWidth` by `kGridWidth` by `kGridWidth` volume of
animated cubes with one indexed draw (20 by 20 by 20 by default). Cube vertices
use vertex input slot 0, while per-instance 3D translation, scale, and color use
slot 1 with NVRHI's instance input rate. An orbiting camera automatically fits
the grid for the current field of view and aspect ratio, while distance fog makes
the depth layers easy to distinguish.

The pass renders directly to the Vulkan swapchain through NVRHI and combines each
back buffer with its own `D32` depth attachment. Slang supplies SPIR-V compilation,
push-constant reflection, and `SV_InstanceID` animation.

Build the repository, then run:

```text
build/bin/instancing.exe
```

Use `--smoke` to exercise the instanced draw and resize path automatically.
