#include "application.h"
#include "log.h"

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
} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path executable_path{ argc > 0 ? argv[0] : "" };
    const auto log_path = executable_path.parent_path() / "logs/ArtiChoco.log";
    int exit_code = EXIT_FAILURE;

    try {
        arti::core::Logger::init(log_path.string(), defaultLogLevel);
        ARTI_CORE_INFO("ArtiChoco starting");

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

    shutdownLogger();
    return exit_code;
}
