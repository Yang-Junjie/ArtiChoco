#include "log.h"

#include <filesystem>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace arti::core {
namespace {

using ChannelRegistry = std::unordered_map<std::string, Logger::ChannelHandle>;

ChannelRegistry& channelRegistry()
{
    static ChannelRegistry channels;
    return channels;
}

std::mutex& channelRegistryMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::vector<spdlog::sink_ptr>& activeSinks()
{
    static std::vector<spdlog::sink_ptr> sinks;
    return sinks;
}

} // namespace

void Logger::init(const std::string& log_file, Level level)
{
    // 文件 sink 建不起来时的原因，留到出锁之后再打 —— 打日志本身要走 channel，在这里打不安全。
    std::string file_sink_error;

    {
        static_cast<void>(core());

        std::scoped_lock state_lock(m_s_mutex);
        std::scoped_lock registry_lock(channelRegistryMutex());

        m_s_initialized.store(false, std::memory_order_release);
        m_s_level.store(level, std::memory_order_release);

        auto& sinks = activeSinks();
        sinks.clear();

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        console_sink->set_pattern("%^[%n] [%l] %v%$");
        sinks.push_back(std::move(console_sink));

        // 文件 sink 是**可选**的：建不起来就只留控制台，不让整个进程起不来。
        //
        // 之前这里的失败会一路抛到 main 的 catch，进程以 EXIT_FAILURE 退出。后果是产物解压到
        // 只读位置（`C:\Program Files\...` 之类）时**根本启动不了**，而报错只有一句
        // "Unhandled exception"，看不出是日志的事 —— 一个诊断设施把它要诊断的程序搞挂了。
        //
        // 日志路径本身没有改：仍然是 exe 旁边的 logs/。真要让只读安装也留下日志，得改成写
        // %LOCALAPPDATA%，那是另一个决定（会挪走开发期习惯的位置），不在这里顺手做。
        if (!log_file.empty()) {
            try {
                const std::filesystem::path log_path{log_file};
                if (log_path.has_parent_path()) {
                    // error_code 重载：目录建不出来是常态失败，不该靠异常表达。
                    std::error_code error;
                    std::filesystem::create_directories(log_path.parent_path(), error);
                    if (error) {
                        throw std::runtime_error("cannot create '" +
                                log_path.parent_path().string() + "': " + error.message());
                    }
                }

                auto file_sink =
                        std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true);
                file_sink->set_level(spdlog::level::trace);
                file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
                sinks.push_back(std::move(file_sink));
            } catch (const std::exception& exception) {
                file_sink_error = exception.what();
            } catch (...) {
                file_sink_error = "unknown error";
            }
        }

        for (const auto& [name, channel] : channelRegistry()) {
            static_cast<void>(name);
            configureChannel(*channel);
        }

        m_s_initialized.store(true, std::memory_order_release);
    }

    // 出锁之后再打。控制台 sink 一定在，所以这条一定看得见。
    if (!file_sink_error.empty()) {
        ARTI_CORE_WARN("Log file '{}' is unavailable ({}); logging to the console only.", log_file,
                file_sink_error);
    }
}

void Logger::shutdown()
{
    std::scoped_lock state_lock(m_s_mutex);
    std::scoped_lock registry_lock(channelRegistryMutex());

    for (const auto& [name, channel] : channelRegistry()) {
        static_cast<void>(name);
        const auto logger = channel->m_logger.exchange(nullptr, std::memory_order_acq_rel);
        if (logger) {
            logger->flush();
        }
    }

    activeSinks().clear();
    m_s_initialized.store(false, std::memory_order_release);
}

bool Logger::isInitialized()
{
    return m_s_initialized.load(std::memory_order_acquire);
}

void Logger::setLevel(Level level)
{
    std::scoped_lock state_lock(m_s_mutex);
    std::scoped_lock registry_lock(channelRegistryMutex());

    m_s_level.store(level, std::memory_order_release);
    const auto spdlog_level = toSpdlogLevel(level);

    for (const auto& [name, channel] : channelRegistry()) {
        static_cast<void>(name);
        const auto logger = channel->m_logger.load(std::memory_order_acquire);
        if (logger) {
            logger->set_level(spdlog_level);
        }
    }
}

Logger::Level Logger::getLevel()
{
    return m_s_level.load(std::memory_order_acquire);
}

Logger::ChannelHandle Logger::registerChannel(std::string name)
{
    if (name.empty()) {
        throw std::invalid_argument("Log channel name cannot be empty.");
    }

    std::scoped_lock lock(channelRegistryMutex());
    auto& channels = channelRegistry();

    if (const auto it = channels.find(name); it != channels.end()) {
        return it->second;
    }

    ChannelHandle channel(new Channel(std::move(name)));
    if (m_s_initialized.load(std::memory_order_acquire)) {
        configureChannel(*channel);
    }

    channels.emplace(channel->getName(), channel);
    return channel;
}

Logger::ChannelHandle Logger::findChannel(std::string_view name)
{
    std::scoped_lock lock(channelRegistryMutex());
    const auto& channels = channelRegistry();
    const auto it = channels.find(std::string{name});
    return it != channels.end() ? it->second : nullptr;
}

const Logger::ChannelHandle& Logger::core()
{
    static const ChannelHandle channel = registerChannel("ArtiCore");
    return channel;
}

void Logger::flush()
{
    std::scoped_lock lock(channelRegistryMutex());
    for (const auto& [name, channel] : channelRegistry()) {
        static_cast<void>(name);
        const auto logger = channel->m_logger.load(std::memory_order_acquire);
        if (logger) {
            logger->flush();
        }
    }
}

void Logger::ensureInitialized()
{
    if (!m_s_initialized.load(std::memory_order_acquire)) {
        init({}, m_s_level.load(std::memory_order_acquire));
    }
}

void Logger::configureChannel(Channel& channel)
{
    auto& sinks = activeSinks();
    auto logger = std::make_shared<spdlog::logger>(channel.getName(), sinks.begin(), sinks.end());
    logger->set_level(toSpdlogLevel(m_s_level.load(std::memory_order_acquire)));
    logger->flush_on(spdlog::level::err);
    channel.m_logger.store(std::move(logger), std::memory_order_release);
}

spdlog::level::level_enum Logger::toSpdlogLevel(Level level)
{
    switch (level) {
        case Level::Trace:
            return spdlog::level::trace;
        case Level::Debug:
            return spdlog::level::debug;
        case Level::Info:
            return spdlog::level::info;
        case Level::Warn:
            return spdlog::level::warn;
        case Level::Error:
            return spdlog::level::err;
        case Level::Fatal:
            return spdlog::level::critical;
        case Level::Off:
            return spdlog::level::off;
    }

    return spdlog::level::off;
}

} // namespace arti::core
