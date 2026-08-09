#include "asset_log.h"

namespace arti::asset {

const core::Logger::Channel& getLogChannel() {
    static const auto channel = core::Logger::registerChannel("ArtiAsset");
    return *channel;
}

} // namespace arti::asset
