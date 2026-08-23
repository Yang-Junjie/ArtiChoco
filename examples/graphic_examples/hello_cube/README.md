# Hello Cube

A rotating textured cube using the ArtiChoco Application + Layer lifecycle,
backend-independent `RenderPass`, NVRHI resources and command recording, Vulkan
presentation, and runtime Slang SPIR-V compilation with reflection.

The supplied `asset/image.png` is uploaded as an sRGB texture and its mip chain is
generated on the GPU through NVRHI.

Build the repository, then run:

```text
build/bin/hello_cube.exe
```

For an automated five-frame run:

```text
build/bin/hello_cube.exe --smoke
```
