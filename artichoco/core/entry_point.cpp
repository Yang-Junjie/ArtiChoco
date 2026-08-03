#include "application.h"
#include "log.h"

#include <memory>

namespace {
#ifdef NDEBUG
constexpr auto defaultLogLevel = arti::core::Logger::Level::Info;
#else
constexpr auto defaultLogLevel = arti::core::Logger::Level::Debug;
#endif
} // namespace

int main(int argc, char** argv)
{
    arti::core::Logger::init("logs/ArtiChoco.log", defaultLogLevel);
    ARTI_CORE_INFO("ArtiChoco starting");

    {
        std::unique_ptr<arti::core::Application> app(arti::core::createApplication(argc, argv));
        ARTI_ASSERT(app, "Failed to create application instance.");

        app->run();
    }

    ARTI_CORE_INFO("ArtiChoco stopped");
    arti::core::Logger::shutdown();
    return 0;
}
