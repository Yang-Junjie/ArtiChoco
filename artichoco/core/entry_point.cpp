#include "application.h"
#include "log.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
#ifdef NDEBUG
constexpr auto defaultLogLevel = arti::core::Logger::Level::Info;
#else
constexpr auto defaultLogLevel = arti::core::Logger::Level::Debug;
#endif

std::filesystem::path resolveLogPath(int argc, char** argv) {
    std::error_code error;
    if (argc > 0 && argv != nullptr && argv[0] != nullptr && argv[0][0] != '\0') {
        const std::filesystem::path executable = std::filesystem::absolute(argv[0], error);
        if (!error && executable.has_parent_path()) {
            return executable.parent_path() / "logs/ArtiChoco.log";
        }
    }
    return std::filesystem::path{ "logs/ArtiChoco.log" };
}

void reportFatalError(const std::filesystem::path& log_path, std::string_view message) noexcept {
    bool logged = false;
    if (arti::core::Logger::isInitialized()) {
        try {
            ARTI_CORE_FATAL("Unhandled exception: {}", message);
            arti::core::Logger::flush();
            logged = true;
        } catch (...) {
        }
    }

    std::fwrite(message.data(), 1, message.size(), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);

    if (!logged) {
        try {
            if (log_path.has_parent_path()) {
                std::filesystem::create_directories(log_path.parent_path());
            }
            std::ofstream output{ log_path, std::ios::app };
            output << "[fatal] " << message << '\n';
        } catch (...) {
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path log_path = resolveLogPath(argc, argv);
    int exit_code = EXIT_FAILURE;
    try {
        arti::core::Logger::init(log_path.string(), defaultLogLevel);
        ARTI_CORE_INFO("ArtiChoco starting");

        std::unique_ptr<arti::core::Application> app(arti::core::createApplication(argc, argv));
        if (!app) {
            throw std::runtime_error("Failed to create application instance.");
        }

        app->run();

        ARTI_CORE_INFO("ArtiChoco stopped");
        exit_code = EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        reportFatalError(log_path, exception.what());
    } catch (...) {
        reportFatalError(log_path, "unknown non-standard exception");
    }

    try {
        arti::core::Logger::shutdown();
    } catch (...) {
    }
    return exit_code;
}
