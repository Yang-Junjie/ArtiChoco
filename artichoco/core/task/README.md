# ArtiChoco Task System

[enkiTS](https://github.com/dougbinks/enkiTS) 的一层封装，位置 `artichoco/core/task/`。
提供阻塞的 fork-join、带句柄的异步任务、钉线程任务，以及一个依赖图。

> **现状：这一层做完了，但还没有任何真实消费者。** 资产管线、剔除 / 抽取、物理、渲染线程
> 目前全是单线程。每个未来接入点的位置和它该调的 API，见 `docs/Architecture/README.md`
> 第 7.1 节那张表。

## 生命周期

进程级单例，显式 init / shutdown，跟 `Logger` 一样。**刻意不做惰性初始化** —— 线程数是
进程启动时的决定，惰性建意味着谁先碰它谁定配置。

```cpp
core::TaskSystem::init(core::TaskSystemConfig{
        .worker_count = 0,           // 0 = 用 enkiTS 的默认（hardware_concurrency - 1）
        .external_thread_count = 0,  // 将来要调 registerExternalThread() 的线程数
        .name_threads = true,        // 起名 ArtiChoco-Worker-<n>，调试器 / profiler 靠它认人
});
// ...
core::TaskSystem::shutdown();
```

- 绑在进程上而不是 `Application` 上：`asset_tools` 那样的 CLI 没有 `Application`，
  绑上去它就拿不到。
- 重复 `init` 是空操作 + 一条 warn。在有任务在跑的时候把线程池拆了重建，比忽略一次多余的
  `init` 危险得多。
- 未 init 时 `get()` 抛 `std::logic_error`，而不是给一个空引用等着被解引用。
- `worker_count` 刻意不自己算 `hardware_concurrency - 1`：那是重复 enkiTS 的策略，
  它哪天改了我们就悄悄漂了。
- **`external_thread_count` 事后补不上** —— enkiTS 在 `Initialize` 时就要知道这个数。
- `shutdown()` 会等所有在途任务跑完（哪怕没人 `wait` 过它们），然后才拆线程。

## 线程编号

`threadIndex()` 就是 enkiTS 的 `GetThreadNum()`：

| 线程 | 下标 |
| --- | --- |
| 调用 `init()` 的那条线程 | `0` |
| 已注册的外部线程 | `[1, 1 + external_thread_count)` |
| enkiTS 自己建的 worker | 其后到 `threadCount()` 为止 |
| 没参与调度的线程（普通 `std::thread`） | `TaskSystem::kNoThread` |

- `threadCount()` = `worker_count + external_thread_count + 1`，**含调用线程**。
- `workerCount()` 是 enkiTS 自己建的 worker 数。**不是 `threadCount() - 1`** ——
  留了外部线程槽位时那个公式会把它们算成 worker。
- `threadIndex()` 的正经用途是**每线程一个输出 bucket**（下标范围 `[0, threadCount())`）。
  不要拿它去改变处理逻辑。

## 阻塞的 fork-join

```cpp
auto& tasks = core::TaskSystem::get();

// 逐元素
tasks.parallelFor(items.size(), [&](uint32_t index) {
    process(items[index]);
}, core::ParallelForOptions{ .min_range = 64 });

// 按分片：每线程一个 bucket，事后合并 —— 这样不用在任务体里加锁
std::vector<std::vector<Result>> buckets(tasks.threadCount());
tasks.parallelForRanges(items.size(),
        [&](uint32_t begin, uint32_t end, uint32_t thread_index) {
            for (uint32_t index = begin; index < end; ++index) {
                buckets[thread_index].push_back(process(items[index]));
            }
        },
        core::ParallelForOptions{ .min_range = 64 });
```

逐元素那个是分片版的糖。两个都是**阻塞**的：返回时活已经干完，任务对象在栈上，不进池。

### `min_range` 要自己填

`ParallelForOptions::min_range` 是 enkiTS 的 grain size（一个分片最少多少个元素）。
默认 `1` 只是为了跟老行为一致，**不是好默认**：

- 每个分片的活少于大约一万个时钟周期时，调度开销能把并行的收益整个吃光。
- 反过来，`min_range >= count` 意味着只有一个分片 —— 那就是单线程，只是绕了一圈。

分片数按 `max(count / partitions, min_range)` 算，`partitions` 由 enkiTS 按线程数定。
经验做法：拿单个元素的大致耗时反推，凑够「每片一万周期左右」。

## 异步任务与句柄

```cpp
core::TaskHandle handle = tasks.submit([&] { doWork(); });
// ... 干别的 ...
tasks.wait(handle);                    // 或者 tasks.isComplete(handle)
```

```cpp
// 异步 parallel-for：Box3D 的任务回调要的就是这个形状
core::TaskHandle handle = tasks.submitParallelFor(count, fn,
        core::ParallelForOptions{ .min_range = 32 });
tasks.wait(handle);
```

句柄是**槽位下标 + 世代号**。三条性质：

- **可以直接丢掉**。任务照跑，跑完槽位自动回收。
- **陈旧句柄是安全的**。槽位被回收再利用时世代号 +1，对不上就当「那个任务早完事了」处理:
  `wait()` 是空操作、`isComplete()` 返回 `true`。不崩、不阻塞、不会误等到别人的任务。
- **`wait()` 期间当前线程会去跑别的任务**（enkiTS 的 `WaitforTask` 就是这么做的），
  不是干等。所以在任务体里 `wait` 另一个任务不会把线程闲死 —— 但要小心递归深度。

### `waitForAll()` 是屏障 / 关停用的，不是通用同步手段

enkiTS 自己的注释写着：在任务被**持续加入**的情况下它不保证有效。所以它只适合
「帧尾屏障」「关停前排空」这两种场景。

**要等「我关心的那批活」，用句柄。** 收集你提交的那些 `TaskHandle` 然后逐个 `wait`，
或者干脆把它们建成一张 `TaskGraph` 等那一个句柄。

## 优先级只有三档

`TaskPriority::High / Normal / Low`。enkiTS 有五档，中间那两档（MED_HI / MED_LO）现在没有
能说清楚的用途，暴露出来只会让调用方在「到底该填哪个」上纠结。

**注意优先级现在只生效了一半。** 另一半靠 enkiTS 的 `WaitforTask(task, priorityOfLowestToRun_)`
—— 高优先级的等待不去跑低优先级的活。那个参数这一层还没暴露，等真有「帧内 / 后台流式」
之分的消费者时再加。所以现在填 `Low` 只影响出队顺序，不能指望它「绝不挤占帧内的活」。

## 钉线程任务

```cpp
// 一次性：在线程 k 上跑一件事
core::TaskHandle handle = tasks.submitPinned(1, [&] { uploadToGPU(data); });
tasks.wait(handle);
```

`thread_index` 必须 `< threadCount()`，否则抛 `std::logic_error`。

**长驻循环**（渲染线程那种）的形状是「提交之后一直不 wait，退出时才 wait」：

```cpp
m_render_thread_task = core::TaskSystem::get().submitPinned(1, [this] { renderThreadLoop(); });
// ... 整个程序生命周期 ...
core::TaskSystem::get().wait(m_render_thread_task);   // 循环自己退出后这里才返回
```

`runPinnedTasks()` 让当前线程把自己的 pinned 队列排空。**普通 `wait()` 不需要先调它** ——
enkiTS 在等待期间会顺手跑本线程的 pinned 任务。需要它的只有「我这条线程只吃 pinned、
自己泵队列」那种循环。

## 外部线程

自己起的线程要参与调度（能被 pin、`threadIndex()` 有意义），得注册：

```cpp
core::TaskSystem::init(core::TaskSystemConfig{ .external_thread_count = 1 });
// ...在那条线程上：
if (core::TaskSystem::get().registerExternalThread()) {   // false = 槽位满了或没留
    // 这里 threadIndex() 落在 [1, 1 + external_thread_count)
    core::TaskSystem::get().deregisterExternalThread();
}
```

槽位数必须在 `init` 时留够。没注册的线程 `threadIndex()` 返回 `kNoThread`。

## 依赖图：`TaskGraph`

**先建图，后提交。整张图一个句柄。**

```cpp
core::TaskGraph graph;
const auto load   = graph.add([&] { bytes = readFile(path); });
const auto decode = graph.addAfter({ load }, [&] { pixels = decodePNG(bytes); });
const auto upload = graph.addPinnedAfter({ decode }, 1, [&] { uploadToGPU(pixels); });
static_cast<void>(upload);

core::TaskHandle handle = tasks.submit(std::move(graph));
tasks.wait(handle);        // 等这一个句柄 = 等整张图
```

节点种类（每种都有 `After` 变体来连前驱）：

| 建节点 | 跑在哪 |
| --- | --- |
| `add` / `addAfter` | 任意 worker |
| `addParallelFor` / `addParallelForAfter` | 拆成分片，多个 worker |
| `addPinned` / `addPinnedAfter` | 指定的那条线程 |

### 三条硬规则

- **没有 `then(runningHandle)`，这是 enkiTS 的限制不是偷懒。** 它的依赖边必须在**前驱入队
  之前**连好；对一个已经在跑或已经跑完的任务调 `SetDependency`，后继会**永远不启动**。
  所以 API 只能是「把整张图建完再提交」。
- **前驱必须是已经 `add` 过的节点。** `addAfter` 只接受下标比自己小的节点，否则抛
  `std::logic_error`。这同时也就排除了环。
- **建图不跑任何东西。** 在 `submit(std::move(graph))` 之前，一个节点都没有入队。
  图被移动走之后再用它，抛 `std::logic_error`。

`wait(handle)` 等的是一个隐式终结节点，它依赖所有出度为 0 的节点 —— 所以调用方不用自己
去收集叶子。提交时只有根节点（没有前驱的）被入队，其余由 enkiTS 在前驱全完成时启动。

## 明确不做的四件事

| 不做 | 理由 | 替代 |
| --- | --- | --- |
| `Future<T>`（带返回值的任务） | 现在没有消费者需要，泛型返回值会把 API 面积翻一倍 | 句柄 + 调用方自己捕获输出变量；并行时用 `threadIndex()` 分 bucket |
| 任务取消 | enkiTS 不支持，硬做要在每个任务体里插检查点 | 自己传一个 `std::atomic<bool>` 令牌在任务体里查（enkiTS 自己也是这个路子） |
| 渲染线程迁移 | 那是帧数据 double buffer、ImGui 线程归属、swapchain 重建时机三件事，比这一层大 | 接缝已经在了：`external_thread_count` + `registerExternalThread()` + 长驻 `submitPinned` |
| 自己写 work-stealing / fiber 调度器 | enkiTS 就是干这个的，而且已经在依赖里 | 无 —— 真要换调度器，`TaskSystem` 这一层的存在意义正是让它可换 |

## 测试

`artichoco/core/tests/task_system_test.cpp`（ctest 目标 `task_system_test`）。

其中一条是**「活真的分到了多条线程上」**：用每线程 bucket 统计非零桶的个数，要求 ≥ 2。
它被反向验过 —— 把 `min_range` 调到等于元素总数（强制单分片）时这条用例确实会失败。
单核机器上 `workerCount() == 0`，那条会跳过而不是失败。

没跑过 TSan：`clang++ -fsanitize=thread` 在 `x86_64-pc-windows-msvc` 上不支持。
