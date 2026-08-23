# Hello Triangle

The minimal ArtiChoco graphics example. It uses the application/layer lifecycle,
the backend-independent `RenderPass` API, NVRHI resources and command recording,
the Vulkan presentation backend, and runtime Slang SPIR-V compilation with
reflection.

Build the repository, then run:

```text
build/bin/hello_triangle.exe
```

For an automated three-frame run:

```text
build/bin/hello_triangle.exe --smoke
```
