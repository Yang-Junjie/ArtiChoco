#include "project_log.h"

namespace arti::project {

const core::Logger::Channel& getLogChannel()
{
    static const auto channel = core::Logger::registerChannel("ArtiProject");
    return *channel;
}

} // namespace arti::project
