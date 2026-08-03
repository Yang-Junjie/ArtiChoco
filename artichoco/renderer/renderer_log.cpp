#include "renderer_log.h"

namespace arti::renderer {

const core::Logger::Channel& getLogChannel()
{
    static const auto channel = core::Logger::registerChannel("ArtiRenderer");
    return *channel;
}

} // namespace arti::renderer
