#pragma once
#include "artichoco/core/window.h"

namespace arti::platform {

std::unique_ptr<core::Window> createSDLWindow(const core::WindowCreateInfo& info);

} // namespace arti::platform
