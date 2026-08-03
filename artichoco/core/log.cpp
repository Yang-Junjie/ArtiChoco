#include "log.h"

#include <filesystem>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <stdexcept>
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

    if (!log_file.empty()) {
        const std::filesystem::path log_path{log_file};
        if (log_path.has_parent_path()) {
            std::filesystem::create_directories(log_path.parent_path());
        }

        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true);
        file_sink->set_level(spdlog::level::trace);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
        sinks.push_back(std::move(file_sink));
    }

    for (const auto& [name, channel] : channelRegistry()) {
        static_cast<void>(name);
        configureChannel(*channel);
    }

    m_s_initialized.store(true, std::memory_order_release);
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
