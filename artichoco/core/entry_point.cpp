#include "application.h"
#include "io/paths.h"
#include "log.h"
#include "task/task_system.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
#ifdef NDEBUG
constexpr auto defaultLogLevel = arti::core::Logger::Level::Info;
#else
constexpr auto defaultLogLevel = arti::core::Logger::Level::Debug;
#endif

void reportFatalError(std::string_view message) noexcept {
    try {
        if (arti::core::Logger::isInitialized()) {
            ARTI_CORE_FATAL("Unhandled exception: {}", message);
        }
    } catch (...) {
        // Reporting must still reach stderr if the logger itself fails.
    }

    std::fwrite(message.data(), 1, message.size(), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

void shutdownLogger() noexcept {
    try {
        arti::core::Logger::shutdown();
    } catch (...) {
        // There is no recovery action to take while the process is exiting.
    }
}

// 任务系统要在日志之前关：worker 退出的路上还会打日志。
void shutdownTaskSystem() noexcept {
    try {
        arti::core::TaskSystem::shutdown();
    } catch (...) {
        // There is no recovery action to take while the process is exiting.
    }
}

// 日志落在 **exe 旁边**，不是 argv[0] 的旁边。从 PATH 启动时 argv[0] 可能只是个文件名，
// 于是日志会掉进当前工作目录；从文件管理器拖文件启动时更不可控。Linux 上 `arti_player`
// 通常就是这么起来的，所以这不是理论问题。
//
// 取不到 exe 路径时才退回 argv[0]：那条路上没有更好的选择，而丢日志比丢进程好。
std::filesystem::path logPath(int argc, char** argv) {
    try {
        return arti::core::executableDir() / "logs/ArtiChoco.log";
    } catch (const std::exception&) {
        const std::filesystem::path executable_path{ argc > 0 ? argv[0] : "" };
        return executable_path.parent_path() / "logs/ArtiChoco.log";
    }
}
} // namespace

int main(int argc, char** argv) {
    const auto log_path = logPath(argc, argv);
    int exit_code = EXIT_FAILURE;

    try {
        arti::core::Logger::init(log_path.string(), defaultLogLevel);
        ARTI_CORE_INFO("ArtiChoco starting");

        // 进程级，和 Logger 一样在建 Application 之前 —— 没有 Application 的进程
        // （asset_tools 那种 CLI）也照样有任务系统可用。
        arti::core::TaskSystem::init();

        auto app = std::unique_ptr<arti::core::Application>{
            arti::core::createApplication(argc, argv)
        };
        if (!app) {
            throw std::runtime_error("Failed to create application instance.");
        }

        app->run();

        ARTI_CORE_INFO("ArtiChoco stopped");
        exit_code = EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        reportFatalError(exception.what());
    } catch (...) {
        reportFatalError("unknown non-standard exception");
    }

    shutdownTaskSystem();
    shutdownLogger();
    return exit_code;
}
