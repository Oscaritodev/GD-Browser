#include "CapabilityMatrix.hpp"

namespace gdbrowser {
    BrowserCapabilities detectPlatformCapabilities() {
        BrowserCapabilities caps;
        caps.tabs = true;
        caps.history = true;
        caps.favorites = true;
        caps.uploads = true;
        caps.permissionPrompts = true;
        caps.externalProtocolFallback = true;
        caps.popupBlocking = true;

#if defined(_WIN32)
        caps.backendName = "Windows WebView2 scaffold";
        caps.fileAccess = false;
#elif defined(__ANDROID__)
        caps.backendName = "Android WebView scaffold";
        caps.fileAccess = false;
#elif defined(__APPLE__)
        caps.backendName = "Apple WebView scaffold";
        caps.fileAccess = false;
#else
        caps.backendName = "Portable scaffold backend";
        caps.fileAccess = false;
#endif

        return caps;
    }

    std::string scaffoldStatusLine(BrowserCapabilities const& capabilities) {
        return fmt::format(
            "{} active. Native renderer bridge is scaffolded, so the shell, persistence, keybinds, and recovery logic are live while embedded rendering stays disabled.",
            capabilities.backendName
        );
    }
}
