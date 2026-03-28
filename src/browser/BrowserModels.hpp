#pragma once

#include <Geode/Geode.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gdbrowser {
    enum class BrowserPermissionKind {
        Audio,
        Popups,
        Clipboard,
        ExternalProtocol,
        FileAccess,
    };

    enum class PermissionDecision {
        Ask,
        Allow,
        Deny,
    };

    enum class DownloadStatus {
        Pending,
        InProgress,
        Completed,
        Failed,
        Cancelled,
    };

    struct BrowserRect {
        float x = 0.f;
        float y = 0.f;
        float width = 0.f;
        float height = 0.f;
    };

    struct BrowserCapabilities {
        std::string backendName;
        bool embeddedView = false;
        bool nativeBridgeReady = false;
        bool tabs = true;
        bool history = true;
        bool favorites = true;
        bool persistentCookies = false;
        bool downloads = false;
        bool uploads = true;
        bool javascriptEval = false;
        bool permissionPrompts = true;
        bool externalProtocolFallback = true;
        bool fileAccess = false;
        bool popupBlocking = true;
    };

    struct BrowserTabState {
        std::string id;
        std::string url;
        std::string title;
        bool restorable = true;
    };

    struct BrowserHistoryEntry {
        std::string url;
        std::string title;
        int visitedAt = 0;
    };

    struct BrowserFavorite {
        std::string url;
        std::string title;
        int addedAt = 0;
    };

    struct SitePermissionRule {
        std::string origin;
        PermissionDecision audio = PermissionDecision::Ask;
        PermissionDecision popups = PermissionDecision::Ask;
        PermissionDecision clipboard = PermissionDecision::Ask;
        PermissionDecision externalProtocol = PermissionDecision::Ask;
        PermissionDecision fileAccess = PermissionDecision::Ask;
    };

    struct DownloadRecord {
        std::string id;
        std::string url;
        std::string suggestedName;
        std::string targetPath;
        DownloadStatus status = DownloadStatus::Pending;
        int createdAt = 0;
        int updatedAt = 0;
        std::string error;
    };

    struct BrowserRuntimeSnapshot {
        BrowserCapabilities capabilities;
        std::vector<BrowserTabState> tabs;
        std::vector<BrowserHistoryEntry> history;
        std::vector<BrowserFavorite> favorites;
        std::vector<DownloadRecord> downloads;
        std::string currentUrl;
        std::string currentTitle;
        std::string lastError;
        std::string statusLine;
        size_t activeTabIndex = 0;
        bool loading = false;
        bool focused = false;
        bool backendAttached = false;
        bool safeModeApplied = false;
        bool usingScaffoldBackend = true;
    };

    std::string toString(BrowserPermissionKind kind);
    std::string toString(PermissionDecision decision);
    std::string toString(DownloadStatus status);
    PermissionDecision parsePermissionDecision(std::string_view value);
    DownloadStatus parseDownloadStatus(std::string_view value);
    std::string normalizeUserUrl(std::string const& raw);
    std::string extractOrigin(std::string const& url);
    bool isHttpLike(std::string_view url);
    int unixNow();
    std::string makeOpaqueId(std::string_view prefix);
    std::string summarizeCapabilities(BrowserCapabilities const& capabilities);
}
