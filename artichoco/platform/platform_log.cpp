#include "platform_log.h"

namespace arti::platform {

const core::Logger::Channel& getLogChannel()
{
    static const auto channel = core::Logger::registerChannel("ArtiPlatform");
    return *channel;
}

} // namespace arti::platform
