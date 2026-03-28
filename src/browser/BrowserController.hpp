#pragma once

#include "BrowserBackend.hpp"
#include "DownloadUploadManager.hpp"
#include "SessionStore.hpp"

#include <memory>

namespace gdbrowser {
    class BrowserController final : public BrowserBackendObserver {
    public:
        static BrowserController& get();

        void attachView(cocos2d::CCNode* host, BrowserRect const& bounds);
        void detachView();
        void updateBounds(BrowserRect const& bounds);
        void navigate(std::string const& rawUrl);
        void goBack();
        void goForward();
        void reload();
        void stop();
        void goHome();
        void newTab(std::string const& url = {});
        void closeTab();
        void selectNextTab(int direction);
        bool toggleFavorite();
        void clearSiteDataForActiveOrigin();
        void openDownloadsFolder();
        void openExternally();
        void requestUpload();
        void tickWatchdog();

        BrowserRuntimeSnapshot snapshot() const;

        void onBrowserUrlChanged(std::string const& url) override;
        void onBrowserTitleChanged(std::string const& title) override;
        void onBrowserLoadStarted(std::string const& url) override;
        void onBrowserLoadFinished(std::string const& url) override;
        void onBrowserLoadFailed(std::string const& url, std::string const& message) override;
        void onBrowserDownloadRequested(std::string const& url, std::string const& suggestedName) override;
        PermissionDecision onBrowserPermissionRequested(std::string const& origin, BrowserPermissionKind kind) override;
        bool onBrowserNewWindowRequested(std::string const& url) override;
        void onBrowserConsoleMessage(std::string const& message) override;
        void onBrowserBackendCrashed(std::string const& reason) override;

    private:
        BrowserController();

        void reloadSettings();
        void ensureInitialized();
        void rebuildBackend(std::string const& reason);
        std::string homepage() const;
        BrowserTabState const& activeTab() const;
        BrowserTabState& activeTab();
        std::string activeOrigin() const;

        SessionStore m_session;
        DownloadUploadManager m_downloads;
        std::unique_ptr<BrowserBackend> m_backend;
        cocos2d::CCNode* m_host = nullptr;
        BrowserRect m_bounds;
        std::string m_currentUrl;
        std::string m_currentTitle;
        std::string m_lastError;
        std::string m_statusLine;
        std::chrono::steady_clock::time_point m_lastLoadStart = std::chrono::steady_clock::now();
        bool m_loading = false;
        bool m_focused = false;
        bool m_attached = false;
        bool m_initialized = false;
        bool m_allowExternalLinks = true;
        bool m_allowFileAccess = false;
        bool m_allowPopups = false;
        bool m_clearOnWatchdog = true;
        bool m_restoreSession = true;
        bool m_safeMode = false;
        bool m_showDebugStatus = true;
        size_t m_historyLimit = 50;
        int m_watchdogSeconds = 15;
    };
}
