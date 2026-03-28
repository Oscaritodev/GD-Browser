#include "BrowserController.hpp"

#include "CapabilityMatrix.hpp"

#include <Geode/utils/file.hpp>
#include <Geode/utils/web.hpp>

namespace gdbrowser {
    BrowserController& BrowserController::get() {
        static BrowserController instance;
        return instance;
    }

    BrowserController::BrowserController() {
        this->reloadSettings();
    }

    void BrowserController::attachView(cocos2d::CCNode* host, BrowserRect const& bounds) {
        this->ensureInitialized();

        m_host = host;
        m_bounds = bounds;
        m_attached = true;
        m_focused = true;

        auto result = m_backend->createView(host, bounds);
        if (!result) {
            m_statusLine = fmt::format("Failed to create backend view: {}", result.unwrapErr());
            return;
        }

        m_backend->setVisible(true);
        m_backend->setFocus(true);

        if (m_currentUrl.empty()) {
            m_currentUrl = this->activeTab().url;
        }
        m_backend->navigate(m_currentUrl);
    }

    void BrowserController::detachView() {
        if (!m_backend) {
            return;
        }
        m_backend->destroyView();
        m_host = nullptr;
        m_attached = false;
        m_focused = false;
    }

    void BrowserController::updateBounds(BrowserRect const& bounds) {
        m_bounds = bounds;
        if (m_backend && m_attached) {
            m_backend->setBounds(bounds);
        }
    }

    void BrowserController::navigate(std::string const& rawUrl) {
        this->ensureInitialized();

        auto normalized = normalizeUserUrl(rawUrl);
        if (normalized.empty()) {
            m_lastError = "The URL is empty.";
            return;
        }

        if (normalized.starts_with("file://") && !m_allowFileAccess) {
            m_lastError = "file:// access is disabled in settings.";
            return;
        }

        m_currentUrl = normalized;
        m_currentTitle = extractOrigin(normalized);
        m_lastError.clear();
        m_session.updateActiveTab(m_currentUrl, m_currentTitle);

        if (m_backend) {
            m_backend->navigate(normalized);
        }
    }

    void BrowserController::goBack() {
        if (m_backend) {
            m_backend->goBack();
        }
    }

    void BrowserController::goForward() {
        if (m_backend) {
            m_backend->goForward();
        }
    }

    void BrowserController::reload() {
        if (m_backend) {
            m_backend->reload();
        }
    }

    void BrowserController::stop() {
        if (m_backend) {
            m_backend->stop();
        }
    }

    void BrowserController::goHome() {
        this->navigate(this->homepage());
    }

    void BrowserController::newTab(std::string const& url) {
        auto target = url.empty() ? this->homepage() : normalizeUserUrl(url);
        m_session.addTab(target, "New Tab");
        m_currentUrl = target;
        m_currentTitle = extractOrigin(target);
        if (m_backend) {
            m_backend->navigate(target);
        }
    }

    void BrowserController::closeTab() {
        m_session.closeActiveTab(this->homepage());
        m_currentUrl = this->activeTab().url;
        m_currentTitle = this->activeTab().title;
        if (m_backend) {
            m_backend->navigate(m_currentUrl);
        }
    }

    void BrowserController::selectNextTab(int direction) {
        auto const& tabs = m_session.tabs();
        if (tabs.empty()) {
            return;
        }

        auto size = static_cast<int>(tabs.size());
        auto index = static_cast<int>(m_session.activeIndex());
        index = (index + direction + size) % size;
        m_session.setActiveIndex(static_cast<size_t>(index));
        m_currentUrl = this->activeTab().url;
        m_currentTitle = this->activeTab().title;
        if (m_backend) {
            m_backend->navigate(m_currentUrl);
        }
    }

    bool BrowserController::toggleFavorite() {
        auto added = m_session.toggleFavorite(m_currentUrl, m_currentTitle);
        m_statusLine = added ? "Added current page to favorites." : "Removed current page from favorites.";
        return added;
    }

    void BrowserController::clearSiteDataForActiveOrigin() {
        auto origin = this->activeOrigin();
        if (origin.empty()) {
            m_lastError = "No active origin to clear.";
            return;
        }

        m_session.clearSiteData(origin);
        if (m_backend) {
            auto result = m_backend->clearSiteData(origin);
            if (!result) {
                m_lastError = fmt::format("Failed to clear site data: {}", result.unwrapErr());
            }
        }
        m_statusLine = fmt::format("Cleared site data for {}", origin);
    }

    void BrowserController::openDownloadsFolder() {
        if (!m_downloads.openDownloadsFolder()) {
            m_lastError = "Could not open the downloads folder.";
            return;
        }
        m_statusLine = "Opened downloads folder.";
    }

    void BrowserController::openExternally() {
        if (m_currentUrl.empty()) {
            return;
        }
        geode::utils::web::openLinkInBrowser(m_currentUrl);
        m_statusLine = "Opened current URL in the system browser.";
    }

    void BrowserController::requestUpload() {
        this->ensureInitialized();

        m_statusLine = "Waiting for file picker...";
        auto picked = m_downloads.pickUploadFile();
        if (!picked) {
            m_statusLine = "Upload picker cancelled.";
            return;
        }

        m_statusLine = fmt::format("Picked upload file: {}. Native upload bridge can consume it once the platform backend is connected.", picked->filename().string());
    }

    void BrowserController::tickWatchdog() {
        if (!m_backend || !m_loading) {
            return;
        }

        auto now = std::chrono::steady_clock::now();
        auto staleFor = std::chrono::duration_cast<std::chrono::seconds>(now - m_backend->lastHeartbeat()).count();
        auto loadingFor = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastLoadStart).count();
        if (staleFor < m_watchdogSeconds || loadingFor < m_watchdogSeconds) {
            return;
        }

        auto origin = this->activeOrigin();
        if (m_clearOnWatchdog && !origin.empty()) {
            m_session.clearSiteData(origin);
            auto clearResult = m_backend->clearSiteData(origin);
            if (!clearResult) {
                geode::log::warn("Watchdog site-data clear failed: {}", clearResult.unwrapErr());
            }
        }

        this->rebuildBackend("Watchdog recovery");
    }

    BrowserRuntimeSnapshot BrowserController::snapshot() const {
        return {
            .capabilities = m_backend ? m_backend->getCapabilities() : detectPlatformCapabilities(),
            .tabs = m_session.tabs(),
            .history = m_session.history(),
            .favorites = m_session.favorites(),
            .downloads = m_downloads.downloads(),
            .currentUrl = m_currentUrl,
            .currentTitle = m_currentTitle,
            .lastError = m_lastError,
            .statusLine = m_statusLine,
            .activeTabIndex = m_session.activeIndex(),
            .loading = m_loading,
            .focused = m_focused,
            .backendAttached = m_attached,
            .safeModeApplied = m_session.safeModeApplied(),
            .usingScaffoldBackend = !(m_backend && m_backend->getCapabilities().embeddedView),
        };
    }

    void BrowserController::onBrowserUrlChanged(std::string const& url) {
        m_currentUrl = url;
        m_session.updateActiveTab(url, m_currentTitle);
    }

    void BrowserController::onBrowserTitleChanged(std::string const& title) {
        m_currentTitle = title.empty() ? m_currentUrl : title;
        m_session.setActiveTitle(m_currentTitle);
    }

    void BrowserController::onBrowserLoadStarted(std::string const& url) {
        m_loading = true;
        m_lastError.clear();
        m_lastLoadStart = std::chrono::steady_clock::now();
        m_statusLine = fmt::format("Loading {}", url);
    }

    void BrowserController::onBrowserLoadFinished(std::string const& url) {
        m_loading = false;
        m_statusLine = fmt::format("Loaded {}", url);
        m_session.recordHistory(url, m_currentTitle, m_historyLimit);
        m_session.updateActiveTab(url, m_currentTitle);
    }

    void BrowserController::onBrowserLoadFailed(std::string const& url, std::string const& message) {
        m_loading = false;
        m_lastError = message;
        m_statusLine = fmt::format("Failed to load {}: {}", url, message);
        geode::log::warn("Browser load failed for {}: {}", url, message);
    }

    void BrowserController::onBrowserDownloadRequested(std::string const& url, std::string const& suggestedName) {
        auto& record = m_downloads.beginDownload(url, suggestedName);
        m_downloads.updateStatus(record.id, DownloadStatus::InProgress);
        m_statusLine = fmt::format("Download queued: {}", record.suggestedName);
        if (!m_backend->getCapabilities().downloads) {
            m_downloads.updateStatus(record.id, DownloadStatus::Failed, "Native download bridge not connected yet.");
        }
    }

    PermissionDecision BrowserController::onBrowserPermissionRequested(std::string const& origin, BrowserPermissionKind kind) {
        auto saved = m_session.decisionFor(origin, kind);
        if (saved != PermissionDecision::Ask) {
            return saved;
        }

        PermissionDecision decision = PermissionDecision::Ask;
        switch (kind) {
            case BrowserPermissionKind::Popups:
                decision = m_allowPopups ? PermissionDecision::Allow : PermissionDecision::Deny;
                break;
            case BrowserPermissionKind::ExternalProtocol:
                decision = m_allowExternalLinks ? PermissionDecision::Allow : PermissionDecision::Deny;
                break;
            case BrowserPermissionKind::FileAccess:
                decision = m_allowFileAccess ? PermissionDecision::Allow : PermissionDecision::Deny;
                break;
            default:
                decision = PermissionDecision::Ask;
                break;
        }

        m_session.setDecision(origin, kind, decision);
        return decision;
    }

    bool BrowserController::onBrowserNewWindowRequested(std::string const& url) {
        if (!m_allowExternalLinks) {
            return false;
        }
        geode::utils::web::openLinkInBrowser(url);
        m_statusLine = fmt::format("Opened unsupported protocol externally: {}", url);
        return true;
    }

    void BrowserController::onBrowserConsoleMessage(std::string const& message) {
        if (m_showDebugStatus) {
            m_statusLine = message;
        }
        geode::log::debug("[GD Browser] {}", message);
    }

    void BrowserController::onBrowserBackendCrashed(std::string const& reason) {
        this->rebuildBackend(fmt::format("Backend crash: {}", reason));
    }

    void BrowserController::reloadSettings() {
        auto* mod = geode::Mod::get();
        m_allowExternalLinks = mod->getSettingValue<bool>("allow-external-links");
        m_allowFileAccess = mod->getSettingValue<bool>("allow-file-access");
        m_allowPopups = mod->getSettingValue<bool>("allow-popups");
        m_clearOnWatchdog = mod->getSettingValue<bool>("clear-cache-on-watchdog");
        m_restoreSession = mod->getSettingValue<bool>("auto-restore-session");
        m_safeMode = mod->getSettingValue<bool>("safe-mode");
        m_showDebugStatus = mod->getSettingValue<bool>("show-debug-status");
        m_historyLimit = static_cast<size_t>(mod->getSettingValue<int64_t>("max-history-items"));
        m_watchdogSeconds = static_cast<int>(mod->getSettingValue<int64_t>("watchdog-seconds"));
    }

    void BrowserController::ensureInitialized() {
        this->reloadSettings();
        if (m_initialized) {
            return;
        }

        m_session.load(m_safeMode, m_restoreSession, this->homepage());
        m_downloads.load();
        m_backend = createPlatformBrowserBackend();
        m_backend->setObserver(this);
        m_currentUrl = this->activeTab().url;
        m_currentTitle = this->activeTab().title;
        m_statusLine = scaffoldStatusLine(m_backend->getCapabilities());
        m_initialized = true;
    }

    void BrowserController::rebuildBackend(std::string const& reason) {
        geode::log::warn("Rebuilding browser backend: {}", reason);

        auto urlToRestore = m_currentUrl.empty() ? this->homepage() : m_currentUrl;
        if (m_backend) {
            m_backend->destroyView();
        }

        m_backend = createPlatformBrowserBackend();
        m_backend->setObserver(this);
        m_statusLine = fmt::format("{}; restored {}", reason, urlToRestore);
        m_loading = false;

        if (m_attached && m_host) {
            auto result = m_backend->createView(m_host, m_bounds);
            if (!result) {
                m_lastError = fmt::format("Backend recovery failed: {}", result.unwrapErr());
                return;
            }
            m_backend->setVisible(true);
            m_backend->setFocus(m_focused);
            m_backend->navigate(urlToRestore);
        }
    }

    std::string BrowserController::homepage() const {
        return geode::Mod::get()->getSettingValue<std::string>("home-page");
    }

    BrowserTabState const& BrowserController::activeTab() const {
        return m_session.activeTab();
    }

    BrowserTabState& BrowserController::activeTab() {
        return m_session.activeTab();
    }

    std::string BrowserController::activeOrigin() const {
        return extractOrigin(m_currentUrl);
    }
}
