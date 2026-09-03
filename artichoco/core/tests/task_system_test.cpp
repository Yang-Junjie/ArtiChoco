// 任务系统的测试。ArtiChoco 的第一个 ctest 目标。
//
// 为什么它比一般的单元测试重要：这一层**没有真实消费者**（见
// docs/tasks/2026-09-02-artichoco-job-system.md 的 D1），所以这个测试就是它唯一的使用者。
// 尤其是「真的跑在多个线程上」那条断言 —— 没有它，一个把所有活都在调用线程上跑完的实现
// 能把其它全部断言都过掉，而那正是这次要修的毛病。

#include "log.h"
#include "task/task_system.h"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// tlhelp32.h 要在 windows.h 之后。
#include <tlhelp32.h>
#endif

namespace {

using arti::core::ParallelForOptions;
using arti::core::TaskGraph;
using arti::core::TaskHandle;
using arti::core::TaskPriority;
using arti::core::TaskSystem;
using arti::core::TaskSystemConfig;

bool require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "task_system_test: " << message << '\n';
    }
    return condition;
}

#if defined(_WIN32)
// 从进程外面枚举线程的名字，而不是「在任务体里读自己的名字」。理由是**确定性**：
// 任务落在哪个线程上由调度器说了算，而 worker 在 init 返回时就已经起好名了。
std::vector<std::wstring> currentProcessThreadNames() {
    std::vector<std::wstring> names;

    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return names;
    }

    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    const DWORD process_id = GetCurrentProcessId();

    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID != process_id) {
                continue;
            }

            const HANDLE thread =
                    OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ThreadID);
            if (thread == nullptr) {
                continue;
            }

            PWSTR description = nullptr;
            if (SUCCEEDED(GetThreadDescription(thread, &description)) && description != nullptr) {
                names.emplace_back(description);
                LocalFree(description);
            }
            CloseHandle(thread);
        } while (Thread32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return names;
}

bool hasThreadNamePrefix(std::wstring_view prefix) {
    for (const auto& name: currentProcessThreadNames()) {
        if (name.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}
#endif

// 未 init 时 get() 必须抛，而不是返回一个空引用让调用方去解引用。
int testGetBeforeInit() {
    if (!require(!TaskSystem::isInitialized(), "进程一开始不该是已初始化状态")) {
        return 1;
    }

    bool threw = false;
    try {
        static_cast<void>(TaskSystem::get());
    } catch (const std::logic_error&) {
        threw = true;
    }
    if (!require(threw, "未 init 时 get() 应该抛 std::logic_error")) {
        return 1;
    }

    return 0;
}

// worker_count 要真的落到 enkiTS 上。threadCount 含调用线程，所以期望值是 worker_count + 1。
int testWorkerCount(uint32_t worker_count) {
    TaskSystem::init(TaskSystemConfig{ .worker_count = worker_count });
    if (!require(TaskSystem::isInitialized(), "init 之后 isInitialized() 应该为真")) {
        return 1;
    }

    const uint32_t expected_threads = worker_count + 1;
    const uint32_t actual_threads = TaskSystem::get().threadCount();
    const uint32_t actual_workers = TaskSystem::get().workerCount();
    if (!require(actual_threads == expected_threads,
                "worker_count=" + std::to_string(worker_count) + " 时 threadCount 应该是 " +
                        std::to_string(expected_threads) + "，实际是 " +
                        std::to_string(actual_threads))) {
        TaskSystem::shutdown();
        return 1;
    }
    if (!require(actual_workers == worker_count,
                "workerCount 应该等于配置的 worker_count，实际 " +
                        std::to_string(actual_workers))) {
        TaskSystem::shutdown();
        return 1;
    }

    // 重复 init 是空操作：线程池是进程级资源，在有任务在跑的时候拆了重建比忽略更危险。
    TaskSystem::init(TaskSystemConfig{ .worker_count = worker_count + 3 });
    if (!require(TaskSystem::get().threadCount() == expected_threads,
                "重复 init 不该换掉已有的 scheduler")) {
        TaskSystem::shutdown();
        return 1;
    }

    TaskSystem::shutdown();
    if (!require(!TaskSystem::isInitialized(), "shutdown 之后 isInitialized() 应该为假")) {
        return 1;
    }

    return 0;
}

// 线程命名。这是「活到底有没有分出去」最便宜的观测手段，所以它值得一条真断言，
// 而不是「在调试器里看一眼」。
int testThreadNaming() {
#if defined(_WIN32)
    TaskSystem::init(TaskSystemConfig{ .worker_count = 2, .name_threads = true });
    const bool named = hasThreadNamePrefix(L"ArtiChoco-Worker-");
    TaskSystem::shutdown();
    if (!require(named, "name_threads=true 时应该能找到 ArtiChoco-Worker-* 线程")) {
        return 1;
    }

    TaskSystem::init(TaskSystemConfig{ .worker_count = 2, .name_threads = false });
    const bool unnamed = !hasThreadNamePrefix(L"ArtiChoco-Worker-");
    TaskSystem::shutdown();
    if (!require(unnamed, "name_threads=false 时不该有 ArtiChoco-Worker-* 线程")) {
        return 1;
    }
#else
    // 非 Windows 上没有可移植的读回接口，起名本身是 best-effort（见 thread_naming.h）。
    std::cerr << "task_system_test: 跳过线程命名断言（非 Windows）\n";
#endif
    return 0;
}

// 旧的 submit() 是**设计上的泄漏**：每次往一个 vector 塞一个任务对象，只有 waitForAll() 会清。
// 这条断言就是那个泄漏的回归 —— 提交十万次并逐个等，槽位数必须有上界而不是跟着次数长。
int testSlotReuse() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 4 });
    auto& tasks = TaskSystem::get();

    constexpr uint32_t kIterations = 100'000;
    std::atomic<uint32_t> counter{ 0 };
    for (uint32_t index = 0; index < kIterations; ++index) {
        const TaskHandle handle =
                tasks.submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
        tasks.wait(handle);
    }

    const uint32_t ran = counter.load(std::memory_order_relaxed);
    const std::size_t slots = tasks.taskSlotCount();
    TaskSystem::shutdown();

    if (!require(ran == kIterations,
                "十万个 submit 应该都跑到，实际 " + std::to_string(ran))) {
        return 1;
    }
    // 上界给得很松：真正要抓的是「跟着迭代次数线性增长」。逐个等的话在途永远只有一个，
    // 所以实际应该就是 1。
    if (!require(slots <= 64, "槽位数该有上界，实际 " + std::to_string(slots))) {
        return 1;
    }

    return 0;
}

// 陈旧句柄：槽位被回收再利用之后，老句柄上 wait / isComplete 都不该崩、不该阻塞。
int testStaleHandle() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 2 });
    auto& tasks = TaskSystem::get();

    std::atomic<uint32_t> counter{ 0 };
    const TaskHandle first = tasks.submit([&counter] { counter.fetch_add(1); });
    tasks.wait(first);

    int failures = 0;
    if (!require(tasks.isComplete(first), "等过之后 isComplete 应该为真")) {
        ++failures;
    }

    // 再 submit 一个：空闲表空了 → 扫描 → first 的槽位被回收、世代号 +1 → 立刻被复用。
    const TaskHandle second = tasks.submit([&counter] { counter.fetch_add(1); });
    tasks.wait(second);

    // 这两条不是实现细节洁癖：不成立的话下面那半个测试就是**空转**（first 还没变成陈旧句柄）。
    if (!require(second.index == first.index && second.generation == first.generation + 1,
                "第二个任务应该复用同一个槽位并让世代号 +1")) {
        ++failures;
    }

    tasks.wait(first);
    if (!require(tasks.isComplete(first), "陈旧句柄应该报「已完成」")) {
        ++failures;
    }

    TaskSystem::shutdown();
    return failures == 0 ? 0 : 1;
}

// waitForAll 是屏障：submit 一批都不等，直接 waitForAll，之后每个句柄都该报完成。
int testWaitForAll() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 4 });
    auto& tasks = TaskSystem::get();

    constexpr uint32_t kCount = 256;
    std::atomic<uint32_t> counter{ 0 };
    std::vector<TaskHandle> handles;
    handles.reserve(kCount);
    for (uint32_t index = 0; index < kCount; ++index) {
        handles.push_back(
                tasks.submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); }));
    }

    tasks.waitForAll();

    int failures = 0;
    const uint32_t ran = counter.load(std::memory_order_relaxed);
    if (!require(ran == kCount, "waitForAll 之后所有任务都该跑完，实际 " + std::to_string(ran))) {
        ++failures;
    }
    for (const TaskHandle handle: handles) {
        if (!tasks.isComplete(handle)) {
            ++failures;
            static_cast<void>(require(false, "waitForAll 之后还有句柄报未完成"));
            break;
        }
    }

    TaskSystem::shutdown();
    return failures == 0 ? 0 : 1;
}

// 阻塞 parallelFor：每个下标写一次，结果必须刚好覆盖 [0, N)。
int testParallelFor() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 4 });
    auto& tasks = TaskSystem::get();

    constexpr uint32_t kCount = 4096;
    std::vector<std::atomic<uint32_t>> seen(kCount);
    for (auto& slot: seen) {
        slot.store(0, std::memory_order_relaxed);
    }

    tasks.parallelFor(kCount, [&seen](uint32_t index) {
        seen[index].fetch_add(1, std::memory_order_relaxed);
    });

    int failures = 0;
    for (uint32_t index = 0; index < kCount; ++index) {
        const uint32_t hits = seen[index].load(std::memory_order_relaxed);
        if (hits != 1) {
            ++failures;
            static_cast<void>(require(false,
                    "parallelFor 下标 " + std::to_string(index) + " 被写了 " +
                            std::to_string(hits) + " 次"));
            break;
        }
    }

    TaskSystem::shutdown();
    return failures == 0 ? 0 : 1;
}

// grain size：min_range = count 时 enkiTS 不能再切，回调必须只进一次。
int testMinRangeOnePartition() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 4 });
    auto& tasks = TaskSystem::get();

    constexpr uint32_t kCount = 1024;
    std::atomic<uint32_t> partitions{ 0 };
    std::atomic<uint32_t> covered{ 0 };

    tasks.parallelForRanges(
            kCount,
            [&](uint32_t begin, uint32_t end, uint32_t) {
                partitions.fetch_add(1, std::memory_order_relaxed);
                covered.fetch_add(end - begin, std::memory_order_relaxed);
            },
            ParallelForOptions{ .min_range = kCount });

    const uint32_t partition_count = partitions.load(std::memory_order_relaxed);
    const uint32_t covered_count = covered.load(std::memory_order_relaxed);
    TaskSystem::shutdown();

    int failures = 0;
    if (!require(partition_count == 1,
                "min_range=count 应该只出一个 partition，实际 " +
                        std::to_string(partition_count))) {
        ++failures;
    }
    if (!require(covered_count == kCount,
                "唯一的 partition 应该覆盖全部 " + std::to_string(kCount) +
                        "，实际 " + std::to_string(covered_count))) {
        ++failures;
    }
    return failures == 0 ? 0 : 1;
}

// min_range == 0 夹到 1，不能让 enkiTS 除零或切出空片。
int testMinRangeZeroClamped() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 2 });
    auto& tasks = TaskSystem::get();

    constexpr uint32_t kCount = 64;
    std::atomic<uint32_t> covered{ 0 };
    tasks.parallelForRanges(
            kCount,
            [&](uint32_t begin, uint32_t end, uint32_t) {
                covered.fetch_add(end - begin, std::memory_order_relaxed);
            },
            ParallelForOptions{ .min_range = 0 });

    const uint32_t covered_count = covered.load(std::memory_order_relaxed);
    TaskSystem::shutdown();

    if (!require(covered_count == kCount,
                "min_range=0 夹到 1 之后仍应覆盖全部，实际 " +
                        std::to_string(covered_count))) {
        return 1;
    }
    return 0;
}

// 异步 parallelFor：物理桥要的就是「拿一个可等待的东西回来」。
int testSubmitParallelFor() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 4 });
    auto& tasks = TaskSystem::get();

    constexpr uint32_t kCount = 2048;
    std::vector<std::atomic<uint32_t>> seen(kCount);
    for (auto& slot: seen) {
        slot.store(0, std::memory_order_relaxed);
    }

    const TaskHandle handle = tasks.submitParallelFor(kCount, [&seen](uint32_t index) {
        seen[index].fetch_add(1, std::memory_order_relaxed);
    });
    if (!require(handle.valid(), "submitParallelFor 应该返回有效句柄")) {
        TaskSystem::shutdown();
        return 1;
    }

    tasks.wait(handle);

    int failures = 0;
    if (!require(tasks.isComplete(handle), "wait 之后 isComplete 应该为真")) {
        ++failures;
    }
    for (uint32_t index = 0; index < kCount; ++index) {
        const uint32_t hits = seen[index].load(std::memory_order_relaxed);
        if (hits != 1) {
            ++failures;
            static_cast<void>(require(false,
                    "submitParallelFor 下标 " + std::to_string(index) + " 被写了 " +
                            std::to_string(hits) + " 次"));
            break;
        }
    }

    TaskSystem::shutdown();
    return failures == 0 ? 0 : 1;
}

// 三档都能提交并完成。优先级的**效果**这一层不做断言（那半边是 WaitforTask 的
// priorityOfLowestToRun_，D5 明确留下）。
int testPriorities() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 2 });
    auto& tasks = TaskSystem::get();

    std::atomic<uint32_t> counter{ 0 };
    const TaskHandle high = tasks.submit(
            [&counter] { counter.fetch_add(1, std::memory_order_relaxed); }, TaskPriority::High);
    const TaskHandle normal = tasks.submit(
            [&counter] { counter.fetch_add(1, std::memory_order_relaxed); }, TaskPriority::Normal);
    const TaskHandle low = tasks.submit(
            [&counter] { counter.fetch_add(1, std::memory_order_relaxed); }, TaskPriority::Low);

    const TaskHandle ranged = tasks.submitParallelForRanges(
            32,
            [&counter](uint32_t begin, uint32_t end, uint32_t) {
                counter.fetch_add(end - begin, std::memory_order_relaxed);
            },
            ParallelForOptions{ .priority = TaskPriority::High });

    tasks.wait(high);
    tasks.wait(normal);
    tasks.wait(low);
    tasks.wait(ranged);

    const uint32_t ran = counter.load(std::memory_order_relaxed);
    TaskSystem::shutdown();

    if (!require(ran == 35, "三档优先级加上一个 High 的 range 任务应该一共跑 35，实际 " +
                            std::to_string(ran))) {
        return 1;
    }
    return 0;
}

// pinned 任务必须落在指定的 enkiTS 线程上。这是旧 launchPinned 单槽位 UAF 的正面用例。
int testPinnedThreadIndex() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 2 });
    auto& tasks = TaskSystem::get();

    std::atomic<uint32_t> observed{ TaskSystem::kNoThread };
    const TaskHandle handle = tasks.submitPinned(1, [&tasks, &observed] {
        observed.store(tasks.threadIndex(), std::memory_order_relaxed);
    });
    tasks.wait(handle);

    const uint32_t thread = observed.load(std::memory_order_relaxed);
    TaskSystem::shutdown();

    if (!require(thread == 1, "submitPinned(1) 应该跑在线程 1 上，实际 " + std::to_string(thread))) {
        return 1;
    }
    return 0;
}

// 旧 API 只有一个 m_pinned_task 槽位，第二次 launchPinned 会把第一次的对象 unique_ptr 掉。
int testPinnedTwice() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 2 });
    auto& tasks = TaskSystem::get();

    std::atomic<uint32_t> counter{ 0 };
    const TaskHandle first = tasks.submitPinned(1, [&counter] {
        counter.fetch_add(1, std::memory_order_relaxed);
    });
    const TaskHandle second = tasks.submitPinned(1, [&counter] {
        counter.fetch_add(1, std::memory_order_relaxed);
    });
    tasks.wait(first);
    tasks.wait(second);

    const uint32_t ran = counter.load(std::memory_order_relaxed);
    TaskSystem::shutdown();

    if (!require(ran == 2, "连续两次 submitPinned 都该跑到，实际 " + std::to_string(ran))) {
        return 1;
    }
    return 0;
}

// 钉在调用线程（0）上：WaitforTask 会顺手跑本线程的 pinned 队列，不需要单独泵。
int testPinnedOnCaller() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 1 });
    auto& tasks = TaskSystem::get();

    std::atomic<uint32_t> observed{ TaskSystem::kNoThread };
    const TaskHandle handle = tasks.submitPinned(0, [&tasks, &observed] {
        observed.store(tasks.threadIndex(), std::memory_order_relaxed);
    });
    tasks.wait(handle);

    const uint32_t thread = observed.load(std::memory_order_relaxed);
    TaskSystem::shutdown();

    if (!require(thread == 0, "submitPinned(0) 应该跑在调用线程上，实际 " + std::to_string(thread))) {
        return 1;
    }
    return 0;
}

// 外部线程：注册后有合法下标、能 submit/wait；注销后回到 kNoThread。
int testExternalThread() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 2, .external_thread_count = 1 });
    auto& tasks = TaskSystem::get();

    int failures = 0;
    if (!require(tasks.threadIndex() == 0, "init 所在线程的 threadIndex 应该是 0")) {
        ++failures;
    }
    if (!require(tasks.threadCount() == 4,
                "2 worker + 1 external + 调用线程，threadCount 应该是 4，实际 " +
                        std::to_string(tasks.threadCount()))) {
        ++failures;
    }
    if (!require(tasks.workerCount() == 2,
                "有外部槽位时 workerCount 仍应是 2，不是 threadCount()-1")) {
        ++failures;
    }

    std::atomic<uint32_t> before{ 0 };
    std::atomic<bool> registered_ok{ false };
    std::atomic<uint32_t> registered_index{ TaskSystem::kNoThread };
    std::atomic<uint32_t> work{ 0 };
    std::atomic<uint32_t> after{ 0 };

    std::thread external([&] {
        before.store(tasks.threadIndex(), std::memory_order_relaxed);
        registered_ok.store(tasks.registerExternalThread());
        registered_index.store(tasks.threadIndex(), std::memory_order_relaxed);

        const TaskHandle handle =
                tasks.submit([&work] { work.fetch_add(1, std::memory_order_relaxed); });
        tasks.wait(handle);

        tasks.deregisterExternalThread();
        after.store(tasks.threadIndex(), std::memory_order_relaxed);
    });
    external.join();

    if (!require(before.load(std::memory_order_relaxed) == TaskSystem::kNoThread,
                "没注册的 std::thread 应该是 kNoThread")) {
        ++failures;
    }
    if (!require(registered_ok.load(std::memory_order_relaxed), "留了 1 个外部槽位，注册应该成功")) {
        ++failures;
    }
    const uint32_t index = registered_index.load(std::memory_order_relaxed);
    if (!require(index != TaskSystem::kNoThread && index < tasks.threadCount(),
                "注册后 threadIndex 应该落在 [1, threadCount)")) {
        ++failures;
    }
    if (!require(work.load(std::memory_order_relaxed) == 1, "外部线程上 submit/wait 应该能跑完任务")) {
        ++failures;
    }
    if (!require(after.load(std::memory_order_relaxed) == TaskSystem::kNoThread,
                "注销后 threadIndex 应该回到 kNoThread")) {
        ++failures;
    }

    TaskSystem::shutdown();
    return failures == 0 ? 0 : 1;
}

int testExternalThreadWithoutSlot() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 1, .external_thread_count = 0 });
    auto& tasks = TaskSystem::get();

    std::atomic<bool> registered_ok{ true };
    std::thread extra([&] { registered_ok.store(tasks.registerExternalThread()); });
    extra.join();

    const bool ok = registered_ok.load(std::memory_order_relaxed);
    TaskSystem::shutdown();

    if (!require(!ok, "没留外部槽位时 registerExternalThread 应该失败")) {
        return 1;
    }
    return 0;
}

// 建图不提交时什么都不该跑。这是 D4 的第一道门：依赖边连上不等于入队。
int testGraphBuildDoesNotRun() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 2 });

    std::atomic<uint32_t> ran{ 0 };
    {
        TaskGraph graph;
        const auto node = graph.add([&ran] { ran.fetch_add(1, std::memory_order_relaxed); });
        static_cast<void>(node);
    }

    const uint32_t value = ran.load(std::memory_order_relaxed);
    TaskSystem::shutdown();
    if (!require(value == 0, "没 submit 的图不该跑，实际跑了 " + std::to_string(value))) {
        return 1;
    }
    return 0;
}

// 菱形 A → {B, C} → D。用原子标志断言顺序，不用 sleep。
int testGraphDiamond() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 4 });
    auto& tasks = TaskSystem::get();

    std::atomic<uint32_t> a_done{ 0 };
    std::atomic<uint32_t> b_done{ 0 };
    std::atomic<uint32_t> c_done{ 0 };
    std::atomic<uint32_t> d_runs{ 0 };
    std::atomic<uint32_t> order_failures{ 0 };

    TaskGraph graph;
    const auto a = graph.add([&] { a_done.store(1, std::memory_order_release); });
    const auto b = graph.addAfter({ a }, [&] {
        if (a_done.load(std::memory_order_acquire) != 1) {
            order_failures.fetch_add(1, std::memory_order_relaxed);
        }
        b_done.store(1, std::memory_order_release);
    });
    const auto c = graph.addAfter({ a }, [&] {
        if (a_done.load(std::memory_order_acquire) != 1) {
            order_failures.fetch_add(1, std::memory_order_relaxed);
        }
        c_done.store(1, std::memory_order_release);
    });
    const auto d = graph.addAfter({ b, c }, [&] {
        if (b_done.load(std::memory_order_acquire) != 1 ||
                c_done.load(std::memory_order_acquire) != 1) {
            order_failures.fetch_add(1, std::memory_order_relaxed);
        }
        d_runs.fetch_add(1, std::memory_order_relaxed);
    });
    static_cast<void>(d);

    const TaskHandle handle = tasks.submit(std::move(graph));
    tasks.wait(handle);

    int failures = 0;
    if (!require(d_runs.load(std::memory_order_relaxed) == 1, "菱形的 D 应该只跑一次")) {
        ++failures;
    }
    if (!require(order_failures.load(std::memory_order_relaxed) == 0,
                "菱形的 B/C 必须在 A 之后，D 必须在 B 和 C 都完成之后")) {
        ++failures;
    }
    if (!require(tasks.isComplete(handle), "等过之后图句柄应该报完成")) {
        ++failures;
    }

    TaskSystem::shutdown();
    return failures == 0 ? 0 : 1;
}

// 层二「解码 + 上传」的形状：worker 上 decode，钉在线程 1 上 upload。
int testGraphPinnedUpload() {
    TaskSystem::init(TaskSystemConfig{ .worker_count = 2 });
    auto& tasks = TaskSystem::get();

    std::atomic<uint32_t> decoded{ 0 };
    std::atomic<uint32_t> uploaded_on{ TaskSystem::kNoThread };
    std::atomic<uint32_t> order_failures{ 0 };

    TaskGraph graph;
    const auto decode = graph.add([&] { decoded.store(1, std::memory_order_release); });
    const auto upload = graph.addPinnedAfter({ decode }, 1, [&] {
        if (decoded.load(std::memory_order_acquire) != 1) {
            order_failures.fetch_add(1, std::memory_order_relaxed);
        }
        uploaded_on.store(tasks.threadIndex(), std::memory_order_relaxed);
    });
    static_cast<void>(upload);

    tasks.wait(tasks.submit(std::move(graph)));

    int failures = 0;
    if (!require(order_failures.load(std::memory_order_relaxed) == 0, "upload 必须在 decode 之后")) {
        ++failures;
    }
    if (!require(uploaded_on.load(std::memory_order_relaxed) == 1,
                "upload 应该钉在线程 1，实际 " +
                        std::to_string(uploaded_on.load(std::memory_order_relaxed)))) {
        ++failures;
    }

    TaskSystem::shutdown();
    return failures == 0 ? 0 : 1;
}

// **本任务的核心验收**：一个足够大的 parallelFor 必须真的被多个线程执行过。
// 没有这一条，「所有活都在调用线程上跑完」的实现也能把其它断言全过掉 —— 而那正是
// 「没有真正意义上的多线程能力」。
int testReallyRunsOnMultipleThreads(uint32_t worker_count) {
    TaskSystem::init(TaskSystemConfig{ .worker_count = worker_count });
    auto& tasks = TaskSystem::get();

    // 每线程一个桶（threadIndex 的正经用途），事后合并，避免测试自己引入共享写。
    std::vector<std::atomic<uint32_t>> hits(tasks.threadCount());
    for (auto& slot: hits) {
        slot.store(0, std::memory_order_relaxed);
    }

    // 活量要够：每个 partition 太轻的话调度器没理由把它分出去。
    constexpr uint32_t kCount = 1u << 16;
    std::atomic<uint64_t> sink{ 0 };
    tasks.parallelForRanges(
            kCount,
            [&](uint32_t begin, uint32_t end, uint32_t thread_index) {
                uint64_t local = 0;
                for (uint32_t index = begin; index < end; ++index) {
                    local += static_cast<uint64_t>(index) * index;
                }
                sink.fetch_add(local, std::memory_order_relaxed);
                hits[thread_index].fetch_add(end - begin, std::memory_order_relaxed);
            },
            ParallelForOptions{ .min_range = 512 });

    uint32_t distinct = 0;
    uint32_t covered = 0;
    for (const auto& slot: hits) {
        const uint32_t value = slot.load(std::memory_order_relaxed);
        if (value > 0) {
            ++distinct;
            covered += value;
        }
    }
    // 注意 workerCount() 是**实际**建出来的 worker 数：配置里的 0 表示「enkiTS 自己定」，
    // 不是「没有 worker」。所以这里只有在机器真的只有一个核时才会走单线程那一支。
    const uint32_t workers = tasks.workerCount();
    TaskSystem::shutdown();

    int failures = 0;
    if (!require(covered == kCount, "所有分片加起来应该正好覆盖 " + std::to_string(kCount) +
                                    "，实际 " + std::to_string(covered))) {
        ++failures;
    }
    if (workers == 0) {
        // 单核机器上只有调用线程，见过一个下标才是正确行为 —— 跳过而不是失败。
        if (!require(distinct == 1, "没有 worker 时只该有一个线程跑过")) {
            ++failures;
        }
        return failures == 0 ? 0 : 1;
    }
    if (!require(distinct >= 2,
                "parallelFor 应该被至少两个不同线程跑过，实际只有 " + std::to_string(distinct) +
                        " 个（worker_count=" + std::to_string(worker_count) + "，实际 worker " +
                        std::to_string(workers) + "）")) {
        ++failures;
    }
    return failures == 0 ? 0 : 1;
}

int run() {
    if (testGetBeforeInit() != 0) {
        return 1;
    }
    if (testWorkerCount(1) != 0) {
        return 1;
    }
    if (testWorkerCount(4) != 0) {
        return 1;
    }
    if (testThreadNaming() != 0) {
        return 1;
    }
    if (testSlotReuse() != 0) {
        return 1;
    }
    if (testStaleHandle() != 0) {
        return 1;
    }
    if (testWaitForAll() != 0) {
        return 1;
    }
    if (testParallelFor() != 0) {
        return 1;
    }
    if (testMinRangeOnePartition() != 0) {
        return 1;
    }
    if (testMinRangeZeroClamped() != 0) {
        return 1;
    }
    if (testSubmitParallelFor() != 0) {
        return 1;
    }
    if (testPriorities() != 0) {
        return 1;
    }
    if (testPinnedThreadIndex() != 0) {
        return 1;
    }
    if (testPinnedTwice() != 0) {
        return 1;
    }
    if (testPinnedOnCaller() != 0) {
        return 1;
    }
    if (testExternalThread() != 0) {
        return 1;
    }
    if (testExternalThreadWithoutSlot() != 0) {
        return 1;
    }
    if (testGraphBuildDoesNotRun() != 0) {
        return 1;
    }
    if (testGraphDiamond() != 0) {
        return 1;
    }
    if (testGraphPinnedUpload() != 0) {
        return 1;
    }
    if (testReallyRunsOnMultipleThreads(4) != 0) {
        return 1;
    }
    // 默认配置（worker_count = 0 → enkiTS 自己定线程数）也要过这一条。
    if (testReallyRunsOnMultipleThreads(0) != 0) {
        return 1;
    }

    std::cout << "task_system_test: ok\n";
    return 0;
}

} // namespace

int main() {
    arti::core::Logger::init();
    const int result = run();
    arti::core::Logger::shutdown();
    return result;
}
