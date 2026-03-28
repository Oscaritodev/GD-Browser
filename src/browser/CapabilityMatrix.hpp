#pragma once

#include "BrowserModels.hpp"

namespace gdbrowser {
    BrowserCapabilities detectPlatformCapabilities();
    std::string scaffoldStatusLine(BrowserCapabilities const& capabilities);
}
