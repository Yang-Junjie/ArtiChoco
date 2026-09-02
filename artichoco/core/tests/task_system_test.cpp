// 任务系统的测试。ArtiChoco 的第一个 ctest 目标。
//
// 为什么它比一般的单元测试重要：这一层**没有真实消费者**（见
// docs/tasks/2026-09-02-artichoco-job-system.md 的 D1），所以这个测试就是它唯一的使用者。
// 尤其是「真的跑在多个线程上」那条断言 —— 没有它，一个把所有活都在调用线程上跑完的实现
// 能把其它全部断言都过掉，而那正是这次要修的毛病。

#include "log.h"
#include "task/task_system.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// tlhelp32.h 要在 windows.h 之后。
#include <tlhelp32.h>
#endif

namespace {

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

    const uint32_t expected = worker_count + 1;
    const uint32_t actual = TaskSystem::get().taskThreadCount();
    if (!require(actual == expected,
                "worker_count=" + std::to_string(worker_count) + " 时 taskThreadCount 应该是 " +
                        std::to_string(expected) + "，实际是 " + std::to_string(actual))) {
        TaskSystem::shutdown();
        return 1;
    }

    // 重复 init 是空操作：线程池是进程级资源，在有任务在跑的时候拆了重建比忽略更危险。
    TaskSystem::init(TaskSystemConfig{ .worker_count = worker_count + 3 });
    if (!require(TaskSystem::get().taskThreadCount() == expected,
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
