#include "BrowserModels.hpp"

#include <Geode/utils/string.hpp>

#include <array>
#include <cctype>
#include <ctime>

namespace gdbrowser {
    namespace {
        std::string_view trim(std::string_view text) {
            auto start = text.find_first_not_of(" \t\r\n");
            if (start == std::string_view::npos) {
                return {};
            }
            auto end = text.find_last_not_of(" \t\r\n");
            return text.substr(start, end - start + 1);
        }

        bool isAsciiAlphaNum(unsigned char ch) {
            return std::isalnum(ch) != 0;
        }

        bool isSearchQuery(std::string_view text) {
            if (text.empty()) {
                return false;
            }
            if (text.find_first_of(" \t\r\n") != std::string_view::npos) {
                return true;
            }
            if (text.find("://") != std::string_view::npos) {
                return false;
            }
            if (text.starts_with("file://")) {
                return false;
            }
            if (text.starts_with("localhost") || text.starts_with("127.0.0.1")) {
                return false;
            }
            if (text.find('.') != std::string_view::npos) {
                return false;
            }
            if (text.find(':') != std::string_view::npos || text.find('/') != std::string_view::npos) {
                return false;
            }
            return true;
        }

        std::string encodeUrlComponent(std::string_view value) {
            std::string encoded;
            encoded.reserve(value.size() * 3);

            for (auto ch : value) {
                auto const byte = static_cast<unsigned char>(ch);
                if (isAsciiAlphaNum(byte) || byte == '-' || byte == '_' || byte == '.' || byte == '~') {
                    encoded.push_back(static_cast<char>(byte));
                } else if (byte == ' ') {
                    encoded.push_back('+');
                } else {
                    encoded += fmt::format("%{:02X}", byte);
                }
            }
            return encoded;
        }

        std::string knownHostShortcut(std::string_view raw) {
            static constexpr std::array<std::pair<std::string_view, std::string_view>, 5> shortcuts {{
                { "google", "https://www.google.com" },
                { "youtube", "https://www.youtube.com" },
                { "github", "https://github.com" },
                { "discord", "https://discord.com" },
                { "geode", "https://docs.geode-sdk.org" },
            }};

            auto lowered = geode::utils::string::toLower(std::string(raw));
            for (auto const& [shortcut, destination] : shortcuts) {
                if (lowered == shortcut) {
                    return std::string(destination);
                }
            }
            return {};
        }
    }

    std::string toString(BrowserPermissionKind kind) {
        switch (kind) {
            case BrowserPermissionKind::Audio: return "audio";
            case BrowserPermissionKind::Popups: return "popups";
            case BrowserPermissionKind::Clipboard: return "clipboard";
            case BrowserPermissionKind::ExternalProtocol: return "external";
            case BrowserPermissionKind::FileAccess: return "file";
        }
        return "unknown";
    }

    std::string toString(PermissionDecision decision) {
        switch (decision) {
            case PermissionDecision::Ask: return "ask";
            case PermissionDecision::Allow: return "allow";
            case PermissionDecision::Deny: return "deny";
        }
        return "ask";
    }

    std::string toString(DownloadStatus status) {
        switch (status) {
            case DownloadStatus::Pending: return "pending";
            case DownloadStatus::InProgress: return "in-progress";
            case DownloadStatus::Completed: return "completed";
            case DownloadStatus::Failed: return "failed";
            case DownloadStatus::Cancelled: return "cancelled";
        }
        return "pending";
    }

    PermissionDecision parsePermissionDecision(std::string_view value) {
        value = trim(value);
        if (value == "allow") {
            return PermissionDecision::Allow;
        }
        if (value == "deny") {
            return PermissionDecision::Deny;
        }
        return PermissionDecision::Ask;
    }

    DownloadStatus parseDownloadStatus(std::string_view value) {
        value = trim(value);
        if (value == "in-progress") {
            return DownloadStatus::InProgress;
        }
        if (value == "completed") {
            return DownloadStatus::Completed;
        }
        if (value == "failed") {
            return DownloadStatus::Failed;
        }
        if (value == "cancelled") {
            return DownloadStatus::Cancelled;
        }
        return DownloadStatus::Pending;
    }

    bool isHttpLike(std::string_view url) {
        return url.starts_with("http://") || url.starts_with("https://");
    }

    std::string normalizeUserUrl(std::string const& raw) {
        auto trimmed = std::string(trim(raw));
        if (trimmed.empty()) {
            return {};
        }

        if (auto shortcut = knownHostShortcut(trimmed); !shortcut.empty()) {
            return shortcut;
        }

        if (trimmed.find("://") == std::string::npos) {
            if (isSearchQuery(trimmed)) {
                return fmt::format("https://www.google.com/search?q={}", encodeUrlComponent(trimmed));
            }
            if (trimmed.starts_with("localhost") || trimmed.starts_with("127.0.0.1")) {
                return fmt::format("http://{}", trimmed);
            }
            return fmt::format("https://{}", trimmed);
        }
        return trimmed;
    }

    std::string extractOrigin(std::string const& url) {
        auto schemePos = url.find("://");
        if (schemePos == std::string::npos) {
            return {};
        }

        auto hostStart = schemePos + 3;
        auto hostEnd = url.find('/', hostStart);
        if (hostEnd == std::string::npos) {
            hostEnd = url.size();
        }
        return url.substr(0, hostEnd);
    }

    int unixNow() {
        return static_cast<int>(std::time(nullptr));
    }

    std::string makeOpaqueId(std::string_view prefix) {
        static int counter = 0;
        return fmt::format("{}-{}-{}", prefix, unixNow(), ++counter);
    }

    std::string summarizeCapabilities(BrowserCapabilities const& capabilities) {
        std::vector<std::string> enabled;
        auto maybeAdd = [&enabled](bool condition, std::string_view label) {
            if (condition) {
                enabled.emplace_back(label);
            }
        };

        maybeAdd(capabilities.tabs, "tabs");
        maybeAdd(capabilities.history, "history");
        maybeAdd(capabilities.favorites, "favorites");
        maybeAdd(capabilities.persistentCookies, "cookies");
        maybeAdd(capabilities.downloads, "downloads");
        maybeAdd(capabilities.uploads, "uploads");
        maybeAdd(capabilities.javascriptEval, "js");
        maybeAdd(capabilities.permissionPrompts, "permissions");
        maybeAdd(capabilities.externalProtocolFallback, "external");
        maybeAdd(capabilities.fileAccess, "file://");

        if (enabled.empty()) {
            return "none";
        }
        return geode::utils::string::join(enabled, ", ");
    }
}
