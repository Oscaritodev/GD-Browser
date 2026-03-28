#pragma once

#include "BrowserModels.hpp"

#include <Geode/Geode.hpp>

#include <chrono>
#include <memory>
#include <string>

namespace gdbrowser {
    class BrowserBackendObserver {
    public:
        virtual ~BrowserBackendObserver() = default;

        virtual void onBrowserUrlChanged(std::string const& url) = 0;
        virtual void onBrowserTitleChanged(std::string const& title) = 0;
        virtual void onBrowserLoadStarted(std::string const& url) = 0;
        virtual void onBrowserLoadFinished(std::string const& url) = 0;
        virtual void onBrowserLoadFailed(std::string const& url, std::string const& message) = 0;
        virtual void onBrowserDownloadRequested(std::string const& url, std::string const& suggestedName) = 0;
        virtual PermissionDecision onBrowserPermissionRequested(std::string const& origin, BrowserPermissionKind kind) = 0;
        virtual bool onBrowserNewWindowRequested(std::string const& url) = 0;
        virtual void onBrowserConsoleMessage(std::string const& message) = 0;
        virtual void onBrowserBackendCrashed(std::string const& reason) = 0;
    };

    class BrowserBackend {
    public:
        virtual ~BrowserBackend() = default;

        void setObserver(BrowserBackendObserver* observer);
        BrowserBackendObserver* observer() const;
        std::chrono::steady_clock::time_point lastHeartbeat() const;

        virtual geode::Result<> createView(cocos2d::CCNode* host, BrowserRect const& bounds) = 0;
        virtual void destroyView() = 0;
        virtual void navigate(std::string const& url) = 0;
        virtual void goBack() = 0;
        virtual void goForward() = 0;
        virtual void reload() = 0;
        virtual void stop() = 0;
        virtual void setBounds(BrowserRect const& bounds) = 0;
        virtual void setVisible(bool visible) = 0;
        virtual void setFocus(bool focused) = 0;
        virtual geode::Result<> clearSiteData(std::string const& origin) = 0;
        virtual geode::Result<std::string> evaluateJS(std::string const& script) = 0;
        virtual BrowserCapabilities getCapabilities() const = 0;

    protected:
        void touchHeartbeat();
        void emitUrlChanged(std::string const& url);
        void emitTitleChanged(std::string const& title);
        void emitLoadStarted(std::string const& url);
        void emitLoadFinished(std::string const& url);
        void emitLoadFailed(std::string const& url, std::string const& message);
        void emitDownloadRequested(std::string const& url, std::string const& suggestedName);
        PermissionDecision emitPermissionRequested(std::string const& origin, BrowserPermissionKind kind);
        bool emitNewWindowRequested(std::string const& url);
        void emitConsoleMessage(std::string const& message);
        void emitBackendCrashed(std::string const& reason);

    private:
        BrowserBackendObserver* m_observer = nullptr;
        std::chrono::steady_clock::time_point m_lastHeartbeat = std::chrono::steady_clock::now();
    };

    std::unique_ptr<BrowserBackend> createPlatformBrowserBackend();
}
