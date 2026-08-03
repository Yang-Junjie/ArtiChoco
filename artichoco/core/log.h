#pragma once

#include <cstdint>

#include <atomic>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <utility>

namespace arti::core {

class Logger {
public:
    enum class Level : uint8_t {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
        Off
    };

    class Channel {
    public:
        const std::string& getName() const
        {
            return m_name;
        }

        template <typename... Args> void trace(spdlog::format_string_t<Args...> format, Args&&... args) const
        {
            Logger::log(*this, Level::Trace, format, std::forward<Args>(args)...);
        }

        template <typename... Args> void debug(spdlog::format_string_t<Args...> format, Args&&... args) const
        {
            Logger::log(*this, Level::Debug, format, std::forward<Args>(args)...);
        }

        template <typename... Args> void info(spdlog::format_string_t<Args...> format, Args&&... args) const
        {
            Logger::log(*this, Level::Info, format, std::forward<Args>(args)...);
        }

        template <typename... Args> void warn(spdlog::format_string_t<Args...> format, Args&&... args) const
        {
            Logger::log(*this, Level::Warn, format, std::forward<Args>(args)...);
        }

        template <typename... Args> void error(spdlog::format_string_t<Args...> format, Args&&... args) const
        {
            Logger::log(*this, Level::Error, format, std::forward<Args>(args)...);
        }

        template <typename... Args> void fatal(spdlog::format_string_t<Args...> format, Args&&... args) const
        {
            Logger::log(*this, Level::Fatal, format, std::forward<Args>(args)...);
        }

    private:
        friend class Logger;

        explicit Channel(std::string name)
            : m_name(std::move(name))
        {}

        std::string m_name;
        std::atomic<std::shared_ptr<spdlog::logger>> m_logger;
    };

    using ChannelHandle = std::shared_ptr<Channel>;

    static void init(const std::string& log_file = {}, Level level = Level::Info);
    static void shutdown();

    static bool isInitialized();
    static void setLevel(Level level);
    static Level getLevel();

    static ChannelHandle registerChannel(std::string name);
    static ChannelHandle findChannel(std::string_view name);
    static const ChannelHandle& core();

    static void flush();

    template <typename... Args>
    static void log(const Channel& channel, Level level, spdlog::format_string_t<Args...> format, Args&&... args)
    {
        ensureInitialized();

        const auto logger = channel.m_logger.load(std::memory_order_acquire);
        if (logger) {
            logger->log(toSpdlogLevel(level), format, std::forward<Args>(args)...);
        }
    }

private:
    static void ensureInitialized();
    static void configureChannel(Channel& channel);
    static spdlog::level::level_enum toSpdlogLevel(Level level);

private:
    static inline std::atomic_bool m_s_initialized{false};
    static inline std::atomic<Level> m_s_level{Level::Info};
    static inline std::mutex m_s_mutex;
};

} // namespace arti::core

#define ARTI_CORE_TRACE(...) ::arti::core::Logger::core()->trace(__VA_ARGS__)
#define ARTI_CORE_DEBUG(...) ::arti::core::Logger::core()->debug(__VA_ARGS__)
#define ARTI_CORE_INFO(...)  ::arti::core::Logger::core()->info(__VA_ARGS__)
#define ARTI_CORE_WARN(...)  ::arti::core::Logger::core()->warn(__VA_ARGS__)
#define ARTI_CORE_ERROR(...) ::arti::core::Logger::core()->error(__VA_ARGS__)
#define ARTI_CORE_FATAL(...) ::arti::core::Logger::core()->fatal(__VA_ARGS__)

#ifndef ARTI_ENABLE_ASSERTS
#ifdef NDEBUG
#define ARTI_ENABLE_ASSERTS 0
#else
#define ARTI_ENABLE_ASSERTS 1
#endif
#endif

#if ARTI_ENABLE_ASSERTS
#define ARTI_ASSERT(condition, ...)                              \
    do {                                                         \
        if (!(condition)) {                                      \
            ARTI_CORE_FATAL("Assertion failed: {}", #condition); \
            __VA_OPT__(ARTI_CORE_FATAL(__VA_ARGS__);)            \
            std::abort();                                        \
        }                                                        \
    } while (false)
#else
#define ARTI_ASSERT(condition, ...) ((void) 0)
#endif
