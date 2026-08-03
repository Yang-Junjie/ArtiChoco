#pragma once

namespace arti::platform {

class SDLPlatform {
public:
    static void acquire();
    static void release();
};

} // namespace arti::platform
