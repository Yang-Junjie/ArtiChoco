# ArtiChoco

ArtiEngine 的基础框架子模块。模块级设计文档在各自目录：

- `artichoco/asset/README.md` —— 资产框架
- `artichoco/scene/README.md` —— ECS
- `artichoco/renderer/README.md` —— Vulkan / NVRHI 渲染器
- `artichoco/core/task/README.md` —— 任务系统

## 任务系统

`arti::core::TaskSystem` 是对 enkiTS 的封装：阻塞的 `parallelFor`、带 `TaskHandle` 的异步
`submit` / `submitParallelFor`、指定线程的 `submitPinned`，以及先建图再提交的 `TaskGraph`。
完整 API、线程编号、grain size、`waitForAll()` 的限制、优先级和不做的范围在
[`artichoco/core/task/README.md`](artichoco/core/task/README.md)。

最常见的「工作线程解码、渲染线程上传」写法是：

```cpp
arti::core::TaskGraph graph;
const auto decode = graph.add([&] { pixels = decode(bytes); });
const auto upload = graph.addPinnedAfter({ decode }, 1, [&] { uploadToGPU(pixels); });
static_cast<void>(upload);

arti::core::TaskHandle handle = arti::core::TaskSystem::get().submit(std::move(graph));
arti::core::TaskSystem::get().wait(handle);
```

依赖边必须在任何节点入队**之前**连好，所以没有 `then(runningHandle)`；图建好后一次
`submit(std::move(graph))`，等返回的整图句柄即可。

`waitForAll()` 只用于帧屏障 / 关停，在任务被持续加入时不保证有效；要等特定工作请等它们的
`TaskHandle`。优先级只公开 `High / Normal / Low` 三档。

明确不做：`Future<T>`、任务取消、渲染线程迁移、自己写 work-stealing / fiber 调度器。
