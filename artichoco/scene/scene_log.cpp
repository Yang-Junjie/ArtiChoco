#include "scene_log.h"

namespace arti::scene {

const core::Logger::Channel& getLogChannel() {
    static const auto channel = core::Logger::registerChannel("ArtiScene");
    return *channel;
}

} // namespace arti::scene
