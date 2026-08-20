# Hello Triangle

This example shows the smallest complete ArtiChoco graphics application:

- `application.cpp` creates the windowed application and installs a layer.
- `HelloTriangleLayer` owns the render device and follows the layer lifecycle.
- `TriangleRenderer` creates the vertex buffer and submits the render pass.
- `TrianglePass` compiles the Slang shader and records Vulkan draw commands.

Build from the repository root:

```powershell
cmake -S . -B build
cmake --build build --target hello_triangle
```

The executable is written under `build/bin` (with a configuration subdirectory for multi-config generators).
