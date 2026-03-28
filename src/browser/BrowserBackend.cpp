#include "BrowserBackend.hpp"

#include "CapabilityMatrix.hpp"

#include <Geode/utils/file.hpp>
#include <Geode/utils/string.hpp>

#ifdef GEODE_IS_WINDOWS
    #include <windows.h>
    #include <objbase.h>
#endif

#if defined(GEODE_IS_WINDOWS) && defined(GD_BROWSER_HAS_WEBVIEW2)
    #include <WebView2.h>
    #include <wrl.h>
    #include <wrl/event.h>
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string_view>
#include <utility>

namespace gdbrowser {
    void BrowserBackend::setObserver(BrowserBackendObserver* observer) {
        m_observer = observer;
    }

    BrowserBackendObserver* BrowserBackend::observer() const {
        return m_observer;
    }

    std::chrono::steady_clock::time_point BrowserBackend::lastHeartbeat() const {
        return m_lastHeartbeat;
    }

    void BrowserBackend::touchHeartbeat() {
        m_lastHeartbeat = std::chrono::steady_clock::now();
    }

    void BrowserBackend::emitUrlChanged(std::string const& url) {
        this->touchHeartbeat();
        if (m_observer) {
            m_observer->onBrowserUrlChanged(url);
        }
    }

    void BrowserBackend::emitTitleChanged(std::string const& title) {
        this->touchHeartbeat();
        if (m_observer) {
            m_observer->onBrowserTitleChanged(title);
        }
    }

    void BrowserBackend::emitLoadStarted(std::string const& url) {
        this->touchHeartbeat();
        if (m_observer) {
            m_observer->onBrowserLoadStarted(url);
        }
    }

    void BrowserBackend::emitLoadFinished(std::string const& url) {
        this->touchHeartbeat();
        if (m_observer) {
            m_observer->onBrowserLoadFinished(url);
        }
    }

    void BrowserBackend::emitLoadFailed(std::string const& url, std::string const& message) {
        this->touchHeartbeat();
        if (m_observer) {
            m_observer->onBrowserLoadFailed(url, message);
        }
    }

    void BrowserBackend::emitDownloadRequested(std::string const& url, std::string const& suggestedName) {
        this->touchHeartbeat();
        if (m_observer) {
            m_observer->onBrowserDownloadRequested(url, suggestedName);
        }
    }

    PermissionDecision BrowserBackend::emitPermissionRequested(std::string const& origin, BrowserPermissionKind kind) {
        this->touchHeartbeat();
        if (m_observer) {
            return m_observer->onBrowserPermissionRequested(origin, kind);
        }
        return PermissionDecision::Ask;
    }

    bool BrowserBackend::emitNewWindowRequested(std::string const& url) {
        this->touchHeartbeat();
        if (m_observer) {
            return m_observer->onBrowserNewWindowRequested(url);
        }
        return false;
    }

    void BrowserBackend::emitConsoleMessage(std::string const& message) {
        this->touchHeartbeat();
        if (m_observer) {
            m_observer->onBrowserConsoleMessage(message);
        }
    }

    void BrowserBackend::emitBackendCrashed(std::string const& reason) {
        this->touchHeartbeat();
        if (m_observer) {
            m_observer->onBrowserBackendCrashed(reason);
        }
    }

    namespace {
        std::string escapeJson(std::string_view value) {
            std::string escaped;
            escaped.reserve(value.size() + 8);
            for (auto ch : value) {
                switch (ch) {
                    case '\\': escaped += "\\\\"; break;
                    case '"': escaped += "\\\""; break;
                    case '\n': escaped += "\\n"; break;
                    case '\r': escaped += "\\r"; break;
                    case '\t': escaped += "\\t"; break;
                    default: escaped.push_back(ch); break;
                }
            }
            return escaped;
        }
    }

    class ScaffoldBrowserBackend final : public BrowserBackend {
    public:
        explicit ScaffoldBrowserBackend(BrowserCapabilities capabilities)
          : m_capabilities(std::move(capabilities)) {}

        geode::Result<> createView(cocos2d::CCNode* host, BrowserRect const& bounds) override {
            m_host = host;
            m_bounds = bounds;
            m_created = true;
            this->touchHeartbeat();
            this->emitConsoleMessage(scaffoldStatusLine(m_capabilities));
            return geode::Ok();
        }

        void destroyView() override {
            m_host = nullptr;
            m_created = false;
            m_visible = false;
            m_focused = false;
            this->touchHeartbeat();
        }

        void navigate(std::string const& rawUrl) override {
            auto url = normalizeUserUrl(rawUrl);
            if (url.empty()) {
                this->emitLoadFailed(rawUrl, "The URL is empty.");
                return;
            }

            auto origin = extractOrigin(url);
            auto isFile = url.starts_with("file://");
            auto isSupportedScheme = isHttpLike(url) || isFile;

            if (!isSupportedScheme) {
                auto allowExternal = this->emitPermissionRequested(origin, BrowserPermissionKind::ExternalProtocol);
                if (allowExternal != PermissionDecision::Deny && this->emitNewWindowRequested(url)) {
                    return;
                }
                this->emitLoadFailed(url, "Unsupported protocol for embedded navigation.");
                return;
            }

            if (isFile && !this->m_capabilities.fileAccess) {
                this->emitLoadFailed(url, "file:// access is disabled.");
                return;
            }

            if (m_historyIndex + 1 < m_history.size()) {
                m_history.resize(m_historyIndex + 1);
            }
            m_history.push_back(url);
            m_historyIndex = m_history.empty() ? 0 : m_history.size() - 1;
            m_currentUrl = url;
            m_loading = true;

            this->emitLoadStarted(url);
            this->emitUrlChanged(url);
            this->emitTitleChanged(origin.empty() ? url : origin);

            auto lowered = geode::utils::string::toLower(url);
            for (auto extension : { ".zip", ".rar", ".7z", ".png", ".jpg", ".jpeg", ".pdf" }) {
                if (lowered.ends_with(extension)) {
                    this->emitDownloadRequested(url, fmt::format("download{}", extension));
                    break;
                }
            }

            m_loading = false;
            this->emitLoadFinished(url);
        }

        void goBack() override {
            if (m_history.empty() || m_historyIndex == 0) {
                return;
            }
            --m_historyIndex;
            this->replayCurrent();
        }

        void goForward() override {
            if (m_history.empty() || m_historyIndex + 1 >= m_history.size()) {
                return;
            }
            ++m_historyIndex;
            this->replayCurrent();
        }

        void reload() override {
            if (m_currentUrl.empty()) {
                return;
            }
            this->emitLoadStarted(m_currentUrl);
            this->emitLoadFinished(m_currentUrl);
        }

        void stop() override {
            if (!m_loading) {
                return;
            }
            m_loading = false;
            this->emitConsoleMessage("Navigation cancelled by shell.");
        }

        void setBounds(BrowserRect const& bounds) override {
            m_bounds = bounds;
            this->touchHeartbeat();
        }

        void setVisible(bool visible) override {
            m_visible = visible;
            this->touchHeartbeat();
        }

        void setFocus(bool focused) override {
            m_focused = focused;
            this->touchHeartbeat();
        }

        geode::Result<> clearSiteData(std::string const& origin) override {
            this->emitConsoleMessage(fmt::format("Scaffold backend cleared site data for {}", origin));
            return geode::Ok();
        }

        geode::Result<std::string> evaluateJS(std::string const&) override {
            return geode::Err("JavaScript evaluation is disabled in scaffold mode.");
        }

        BrowserCapabilities getCapabilities() const override {
            return m_capabilities;
        }

    private:
        void replayCurrent() {
            if (m_history.empty() || m_historyIndex >= m_history.size()) {
                return;
            }
            m_currentUrl = m_history[m_historyIndex];
            this->emitLoadStarted(m_currentUrl);
            this->emitUrlChanged(m_currentUrl);
            this->emitTitleChanged(extractOrigin(m_currentUrl));
            this->emitLoadFinished(m_currentUrl);
        }

        BrowserCapabilities m_capabilities;
        cocos2d::CCNode* m_host = nullptr;
        BrowserRect m_bounds;
        std::vector<std::string> m_history;
        size_t m_historyIndex = 0;
        std::string m_currentUrl;
        bool m_created = false;
        bool m_visible = false;
        bool m_focused = false;
        bool m_loading = false;
    };

#if defined(GEODE_IS_WINDOWS) && defined(GD_BROWSER_HAS_WEBVIEW2)
    namespace {
        using Microsoft::WRL::Callback;
        using Microsoft::WRL::ComPtr;

        std::wstring widen(std::string const& text) {
            if (text.empty()) {
                return {};
            }

            auto length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            if (length <= 0) {
                return std::wstring(text.begin(), text.end());
            }

            std::wstring result(static_cast<size_t>(length) - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), length);
            return result;
        }

        std::string narrow(std::wstring const& text) {
            if (text.empty()) {
                return {};
            }

            auto length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (length <= 0) {
                return std::string(text.begin(), text.end());
            }

            std::string result(static_cast<size_t>(length) - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), length, nullptr, nullptr);
            return result;
        }

        std::string takeComString(wchar_t* value) {
            if (!value) {
                return {};
            }
            std::wstring wide = value;
            CoTaskMemFree(value);
            return narrow(wide);
        }

        std::string hrMessage(std::string_view action, HRESULT hr) {
            return fmt::format("{} (0x{:08X})", action, static_cast<unsigned long>(hr));
        }

        std::string webErrorToString(COREWEBVIEW2_WEB_ERROR_STATUS status) {
            switch (status) {
                case COREWEBVIEW2_WEB_ERROR_STATUS_SERVER_UNREACHABLE: return "The server is unreachable.";
                case COREWEBVIEW2_WEB_ERROR_STATUS_TIMEOUT: return "The request timed out.";
                case COREWEBVIEW2_WEB_ERROR_STATUS_HOST_NAME_NOT_RESOLVED: return "The domain name could not be resolved.";
                case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_EXPIRED: return "The site certificate has expired.";
                case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_IS_INVALID: return "The site certificate is invalid.";
                case COREWEBVIEW2_WEB_ERROR_STATUS_CONNECTION_ABORTED: return "The connection was aborted.";
                case COREWEBVIEW2_WEB_ERROR_STATUS_CONNECTION_RESET: return "The connection was reset.";
                case COREWEBVIEW2_WEB_ERROR_STATUS_DISCONNECTED: return "The network connection was lost.";
                case COREWEBVIEW2_WEB_ERROR_STATUS_CANNOT_CONNECT: return "The browser could not connect to this site.";
                case COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED: return "The navigation was cancelled.";
                default: return fmt::format("Navigation failed with WebView2 status {}.", static_cast<int>(status));
            }
        }

        std::optional<BrowserPermissionKind> mapPermissionKind(COREWEBVIEW2_PERMISSION_KIND kind) {
            switch (kind) {
                case COREWEBVIEW2_PERMISSION_KIND_AUTOPLAY: return BrowserPermissionKind::Audio;
                case COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ: return BrowserPermissionKind::Clipboard;
                case COREWEBVIEW2_PERMISSION_KIND_FILE_READ_WRITE: return BrowserPermissionKind::FileAccess;
                default: return std::nullopt;
            }
        }

        COREWEBVIEW2_PERMISSION_STATE toCorePermissionState(PermissionDecision decision) {
            switch (decision) {
                case PermissionDecision::Allow: return COREWEBVIEW2_PERMISSION_STATE_ALLOW;
                case PermissionDecision::Deny: return COREWEBVIEW2_PERMISSION_STATE_DENY;
                case PermissionDecision::Ask: return COREWEBVIEW2_PERMISSION_STATE_DEFAULT;
            }
            return COREWEBVIEW2_PERMISSION_STATE_DEFAULT;
        }

        struct WindowSearchState {
            DWORD pid = 0;
            HWND hwnd = nullptr;
        };

        BOOL CALLBACK findWindowForProcess(HWND hwnd, LPARAM userData) {
            auto* state = reinterpret_cast<WindowSearchState*>(userData);
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != state->pid || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) {
                return TRUE;
            }

            state->hwnd = hwnd;
            return FALSE;
        }

        HWND resolveGameWindow() {
            auto hwnd = GetForegroundWindow();
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid == GetCurrentProcessId()) {
                return hwnd;
            }

            WindowSearchState state { .pid = GetCurrentProcessId(), .hwnd = nullptr };
            EnumWindows(findWindowForProcess, reinterpret_cast<LPARAM>(&state));
            return state.hwnd;
        }

        RECT computeNodeRectInParent(HWND parent, cocos2d::CCNode* host, BrowserRect const& bounds) {
            RECT client {};
            GetClientRect(parent, &client);

            auto clientWidth = std::max(1L, client.right - client.left);
            auto clientHeight = std::max(1L, client.bottom - client.top);

            auto viewSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
            auto bottomLeft = host ? host->convertToWorldSpace({ 0.f, 0.f }) : cocos2d::CCPoint { bounds.x, bounds.y };
            auto topRight = host
                ? host->convertToWorldSpace({ host->getContentSize().width, host->getContentSize().height })
                : cocos2d::CCPoint { bounds.x + bounds.width, bounds.y + bounds.height };

            auto scaleX = static_cast<float>(clientWidth) / std::max(viewSize.width, 1.f);
            auto scaleY = static_cast<float>(clientHeight) / std::max(viewSize.height, 1.f);

            auto left = static_cast<LONG>(std::lround(bottomLeft.x * scaleX));
            auto right = static_cast<LONG>(std::lround(topRight.x * scaleX));
            auto top = static_cast<LONG>(std::lround(clientHeight - topRight.y * scaleY));
            auto bottom = static_cast<LONG>(std::lround(clientHeight - bottomLeft.y * scaleY));

            if (right <= left) {
                right = left + std::max<LONG>(1, static_cast<LONG>(std::lround(bounds.width * scaleX)));
            }
            if (bottom <= top) {
                bottom = top + std::max<LONG>(1, static_cast<LONG>(std::lround(bounds.height * scaleY)));
            }

            return { left, top, right, bottom };
        }
    }

    class WebView2BrowserBackend final : public BrowserBackend {
    public:
        WebView2BrowserBackend() {
            m_capabilities = detectPlatformCapabilities();
            m_capabilities.backendName = "Windows WebView2";
            m_capabilities.embeddedView = true;
            m_capabilities.persistentCookies = true;
            m_capabilities.downloads = true;
            m_capabilities.uploads = true;
            m_capabilities.fileAccess = true;
        }

        ~WebView2BrowserBackend() override {
            this->destroyView();
        }

        geode::Result<> createView(cocos2d::CCNode* host, BrowserRect const& bounds) override {
            m_host = host;
            m_bounds = bounds;
            m_gameWindow = resolveGameWindow();
            if (!m_gameWindow) {
                return geode::Err("Unable to find the Geometry Dash window.");
            }

            if (!m_comInitialized) {
                auto hr = OleInitialize(nullptr);
                if (SUCCEEDED(hr) || hr == S_FALSE) {
                    m_comInitialized = true;
                }
            }

            if (!m_hostWindow) {
                m_hostWindow = CreateWindowExW(
                    0,
                    L"Static",
                    nullptr,
                    WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE,
                    0,
                    0,
                    16,
                    16,
                    m_gameWindow,
                    nullptr,
                    GetModuleHandleW(nullptr),
                    nullptr
                );

                if (!m_hostWindow) {
                    return geode::Err("Failed to create the native browser host window.");
                }
            }

            this->applyWindowBounds();
            this->applyVisibility();
            this->initializeEnvironmentIfNeeded();
            this->emitConsoleMessage("Connecting Windows WebView2...");
            return geode::Ok();
        }

        void destroyView() override {
            this->removeEventHandlers();
            m_webView13.Reset();
            m_webView.Reset();

            if (m_controller) {
                m_controller->Close();
            }
            m_controller4.Reset();
            m_controller2.Reset();
            m_controller.Reset();
            m_environment.Reset();

            if (m_hostWindow) {
                DestroyWindow(m_hostWindow);
                m_hostWindow = nullptr;
            }

            if (m_comInitialized) {
                OleUninitialize();
                m_comInitialized = false;
            }
        }

        void navigate(std::string const& rawUrl) override {
            auto url = normalizeUserUrl(rawUrl);
            if (url.empty()) {
                this->emitLoadFailed(rawUrl, "The URL is empty.");
                return;
            }

            auto isFile = url.starts_with("file://");
            if (!isHttpLike(url) && !isFile) {
                auto origin = extractOrigin(url);
                auto allowExternal = this->emitPermissionRequested(origin, BrowserPermissionKind::ExternalProtocol);
                if (allowExternal != PermissionDecision::Deny && this->emitNewWindowRequested(url)) {
                    return;
                }
                this->emitLoadFailed(url, "Unsupported protocol for embedded navigation.");
                return;
            }

            m_pendingNavigation = widen(url);
            if (m_webView) {
                auto hr = m_webView->Navigate(m_pendingNavigation.c_str());
                if (FAILED(hr)) {
                    this->emitLoadFailed(url, hrMessage("WebView2 could not navigate", hr));
                }
            }
        }

        void goBack() override {
            if (!m_webView) {
                return;
            }

            BOOL canGoBack = FALSE;
            if (SUCCEEDED(m_webView->get_CanGoBack(&canGoBack)) && canGoBack) {
                m_webView->GoBack();
            }
        }

        void goForward() override {
            if (!m_webView) {
                return;
            }

            BOOL canGoForward = FALSE;
            if (SUCCEEDED(m_webView->get_CanGoForward(&canGoForward)) && canGoForward) {
                m_webView->GoForward();
            }
        }

        void reload() override {
            if (m_webView) {
                m_webView->Reload();
            }
        }

        void stop() override {
            if (m_webView) {
                m_webView->Stop();
            }
        }

        void setBounds(BrowserRect const& bounds) override {
            m_bounds = bounds;
            this->applyWindowBounds();
        }

        void setVisible(bool visible) override {
            m_visible = visible;
            this->applyVisibility();
        }

        void setFocus(bool focused) override {
            m_focused = focused;
            if (focused && m_hostWindow) {
                SetFocus(m_hostWindow);
            }
            if (focused && m_controller) {
                m_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            }
        }

        geode::Result<> clearSiteData(std::string const& origin) override {
            if (!m_webView) {
                return geode::Err("The native browser is not ready yet.");
            }

            auto payload = fmt::format(
                "{{\"origin\":\"{}\",\"storageTypes\":\"all\"}}",
                escapeJson(origin)
            );

            auto hr = m_webView->CallDevToolsProtocolMethod(
                L"Storage.clearDataForOrigin",
                widen(payload).c_str(),
                Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
                    [this, origin](HRESULT result, LPCWSTR) -> HRESULT {
                        if (SUCCEEDED(result)) {
                            this->emitConsoleMessage(fmt::format("Cleared native site data for {}", origin));
                        }
                        return S_OK;
                    }
                ).Get()
            );

            if (FAILED(hr)) {
                return geode::Err(hrMessage("WebView2 could not clear site data", hr));
            }
            return geode::Ok();
        }

        geode::Result<std::string> evaluateJS(std::string const&) override {
            return geode::Err("The current bridge does not expose synchronous JavaScript evaluation yet.");
        }

        BrowserCapabilities getCapabilities() const override {
            auto caps = m_capabilities;
            caps.nativeBridgeReady = m_controller && m_webView;
            return caps;
        }

    private:
        void initializeEnvironmentIfNeeded() {
            if (m_environment || !m_hostWindow) {
                if (m_environment && !m_controller) {
                    this->createController();
                }
                return;
            }

            auto userDataDir = geode::Mod::get()->getSaveDir() / "webview2-profile";
            auto ensured = geode::utils::file::createDirectoryAll(userDataDir);
            if (!ensured) {
                this->emitConsoleMessage(fmt::format("Could not create WebView2 profile directory: {}", ensured.unwrapErr()));
            }
            m_userDataDir = userDataDir;

            auto hr = CreateCoreWebView2EnvironmentWithOptions(
                nullptr,
                m_userDataDir.wstring().c_str(),
                nullptr,
                Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                    [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                        if (!m_hostWindow) {
                            return S_OK;
                        }
                        if (FAILED(result) || !environment) {
                            this->emitLoadFailed(
                                narrow(m_pendingNavigation),
                                hrMessage("Failed to create the WebView2 environment", result)
                            );
                            return S_OK;
                        }

                        m_environment = environment;
                        this->createController();
                        return S_OK;
                    }
                ).Get()
            );

            if (FAILED(hr)) {
                this->emitLoadFailed(narrow(m_pendingNavigation), hrMessage("WebView2 environment startup failed", hr));
            }
        }

        void createController() {
            if (!m_environment || !m_hostWindow || m_controller) {
                return;
            }

            auto hr = m_environment->CreateCoreWebView2Controller(
                m_hostWindow,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                        if (!m_hostWindow) {
                            return S_OK;
                        }
                        if (FAILED(result) || !controller) {
                            this->emitLoadFailed(
                                narrow(m_pendingNavigation),
                                hrMessage("Failed to create the WebView2 controller", result)
                            );
                            return S_OK;
                        }

                        m_controller = controller;
                        m_controller.As(&m_controller2);
                        m_controller.As(&m_controller4);

                        if (m_controller2) {
                            COREWEBVIEW2_COLOR background { 255, 255, 255, 255 };
                            m_controller2->put_DefaultBackgroundColor(background);
                        }

                        HRESULT coreHr = m_controller->get_CoreWebView2(&m_webView);
                        if (FAILED(coreHr) || !m_webView) {
                            this->emitLoadFailed(
                                narrow(m_pendingNavigation),
                                hrMessage("Failed to retrieve the WebView2 core view", coreHr)
                            );
                            return S_OK;
                        }

                        m_webView.As(&m_webView13);
                        this->configureSettings();
                        this->registerEventHandlers();
                        this->applyWindowBounds();
                        this->applyVisibility();

                        if (m_focused) {
                            this->setFocus(true);
                        }

                        this->emitConsoleMessage("Windows WebView2 is ready.");
                        if (!m_pendingNavigation.empty()) {
                            m_webView->Navigate(m_pendingNavigation.c_str());
                        }
                        return S_OK;
                    }
                ).Get()
            );

            if (FAILED(hr)) {
                this->emitLoadFailed(narrow(m_pendingNavigation), hrMessage("WebView2 controller startup failed", hr));
            }
        }

        void configureSettings() {
            ComPtr<ICoreWebView2Settings> settings;
            if (!m_webView || FAILED(m_webView->get_Settings(&settings)) || !settings) {
                return;
            }

            settings->put_IsStatusBarEnabled(FALSE);
            settings->put_AreDefaultContextMenusEnabled(FALSE);
            settings->put_AreDevToolsEnabled(FALSE);
            settings->put_AreDefaultScriptDialogsEnabled(TRUE);
            settings->put_IsZoomControlEnabled(TRUE);
        }

        void registerEventHandlers() {
            if (!m_webView) {
                return;
            }

            m_webView->add_NavigationStarting(
                Callback<ICoreWebView2NavigationStartingEventHandler>(
                    [this](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                        LPWSTR value = nullptr;
                        args->get_Uri(&value);
                        auto url = takeComString(value);

                        auto isFile = url.starts_with("file://");
                        if (!isHttpLike(url) && !isFile) {
                            auto origin = extractOrigin(url);
                            auto allowExternal = this->emitPermissionRequested(origin, BrowserPermissionKind::ExternalProtocol);
                            if (allowExternal != PermissionDecision::Deny && this->emitNewWindowRequested(url)) {
                                args->put_Cancel(TRUE);
                                return S_OK;
                            }
                        }

                        this->emitLoadStarted(url);
                        return S_OK;
                    }
                ).Get(),
                &m_navigationStartingToken
            );

            m_webView->add_SourceChanged(
                Callback<ICoreWebView2SourceChangedEventHandler>(
                    [this](ICoreWebView2*, ICoreWebView2SourceChangedEventArgs*) -> HRESULT {
                        LPWSTR value = nullptr;
                        if (m_webView) {
                            m_webView->get_Source(&value);
                        }
                        this->emitUrlChanged(takeComString(value));
                        return S_OK;
                    }
                ).Get(),
                &m_sourceChangedToken
            );

            m_webView->add_DocumentTitleChanged(
                Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
                    [this](ICoreWebView2*, IUnknown*) -> HRESULT {
                        LPWSTR value = nullptr;
                        if (m_webView) {
                            m_webView->get_DocumentTitle(&value);
                        }
                        this->emitTitleChanged(takeComString(value));
                        return S_OK;
                    }
                ).Get(),
                &m_documentTitleChangedToken
            );

            m_webView->add_NavigationCompleted(
                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                    [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                        LPWSTR value = nullptr;
                        if (m_webView) {
                            m_webView->get_Source(&value);
                        }
                        auto url = takeComString(value);

                        BOOL isSuccess = FALSE;
                        args->get_IsSuccess(&isSuccess);
                        if (isSuccess) {
                            this->emitLoadFinished(url);
                            return S_OK;
                        }

                        COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
                        args->get_WebErrorStatus(&status);
                        this->emitLoadFailed(url, webErrorToString(status));
                        return S_OK;
                    }
                ).Get(),
                &m_navigationCompletedToken
            );

            m_webView->add_NewWindowRequested(
                Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                    [this](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                        LPWSTR value = nullptr;
                        args->get_Uri(&value);
                        auto url = takeComString(value);

                        if (this->emitNewWindowRequested(url)) {
                            args->put_Handled(TRUE);
                        }
                        return S_OK;
                    }
                ).Get(),
                &m_newWindowRequestedToken
            );

            m_webView->add_PermissionRequested(
                Callback<ICoreWebView2PermissionRequestedEventHandler>(
                    [this](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT {
                        COREWEBVIEW2_PERMISSION_KIND rawKind = COREWEBVIEW2_PERMISSION_KIND_UNKNOWN_PERMISSION;
                        args->get_PermissionKind(&rawKind);

                        auto kind = mapPermissionKind(rawKind);
                        if (!kind) {
                            return S_OK;
                        }

                        LPWSTR value = nullptr;
                        args->get_Uri(&value);
                        auto origin = extractOrigin(takeComString(value));
                        auto decision = this->emitPermissionRequested(origin, *kind);
                        args->put_State(toCorePermissionState(decision));
                        return S_OK;
                    }
                ).Get(),
                &m_permissionRequestedToken
            );

            ComPtr<ICoreWebView2_4> webView4;
            if (SUCCEEDED(m_webView.As(&webView4)) && webView4) {
                webView4->add_DownloadStarting(
                    Callback<ICoreWebView2DownloadStartingEventHandler>(
                        [this](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT {
                            ComPtr<ICoreWebView2DownloadOperation> downloadOperation;
                            std::string url;
                            std::string fileName = "download.bin";

                            if (SUCCEEDED(args->get_DownloadOperation(&downloadOperation)) && downloadOperation) {
                                LPWSTR value = nullptr;
                                downloadOperation->get_Uri(&value);
                                url = takeComString(value);
                            }

                            LPWSTR pathValue = nullptr;
                            args->get_ResultFilePath(&pathValue);
                            auto resultPath = takeComString(pathValue);
                            if (!resultPath.empty()) {
                                fileName = std::filesystem::path(resultPath).filename().string();
                            }

                            this->emitDownloadRequested(url, fileName);
                            return S_OK;
                        }
                    ).Get(),
                    &m_downloadStartingToken
                );
            }
        }

        void removeEventHandlers() {
            if (!m_webView) {
                return;
            }

            if (m_navigationStartingToken.value) {
                m_webView->remove_NavigationStarting(m_navigationStartingToken);
                m_navigationStartingToken = {};
            }
            if (m_sourceChangedToken.value) {
                m_webView->remove_SourceChanged(m_sourceChangedToken);
                m_sourceChangedToken = {};
            }
            if (m_navigationCompletedToken.value) {
                m_webView->remove_NavigationCompleted(m_navigationCompletedToken);
                m_navigationCompletedToken = {};
            }
            if (m_documentTitleChangedToken.value) {
                m_webView->remove_DocumentTitleChanged(m_documentTitleChangedToken);
                m_documentTitleChangedToken = {};
            }
            if (m_newWindowRequestedToken.value) {
                m_webView->remove_NewWindowRequested(m_newWindowRequestedToken);
                m_newWindowRequestedToken = {};
            }
            if (m_permissionRequestedToken.value) {
                m_webView->remove_PermissionRequested(m_permissionRequestedToken);
                m_permissionRequestedToken = {};
            }

            ComPtr<ICoreWebView2_4> webView4;
            if (m_downloadStartingToken.value && SUCCEEDED(m_webView.As(&webView4)) && webView4) {
                webView4->remove_DownloadStarting(m_downloadStartingToken);
                m_downloadStartingToken = {};
            }
        }

        void applyWindowBounds() {
            if (!m_gameWindow || !m_hostWindow) {
                return;
            }

            auto rect = computeNodeRectInParent(m_gameWindow, m_host, m_bounds);
            auto width = std::max<LONG>(1, rect.right - rect.left);
            auto height = std::max<LONG>(1, rect.bottom - rect.top);

            SetWindowPos(
                m_hostWindow,
                HWND_TOP,
                rect.left,
                rect.top,
                width,
                height,
                SWP_NOACTIVATE
            );

            if (m_controller) {
                RECT localBounds { 0, 0, width, height };
                m_controller->put_Bounds(localBounds);
            }
        }

        void applyVisibility() {
            if (m_hostWindow) {
                ShowWindow(m_hostWindow, m_visible ? SW_SHOW : SW_HIDE);
            }
            if (m_controller) {
                m_controller->put_IsVisible(m_visible ? TRUE : FALSE);
            }
        }

        BrowserCapabilities m_capabilities;
        cocos2d::CCNode* m_host = nullptr;
        BrowserRect m_bounds;
        HWND m_gameWindow = nullptr;
        HWND m_hostWindow = nullptr;
        ComPtr<ICoreWebView2Environment> m_environment;
        ComPtr<ICoreWebView2Controller> m_controller;
        ComPtr<ICoreWebView2Controller2> m_controller2;
        ComPtr<ICoreWebView2Controller4> m_controller4;
        ComPtr<ICoreWebView2> m_webView;
        ComPtr<ICoreWebView2_13> m_webView13;
        std::filesystem::path m_userDataDir;
        std::wstring m_pendingNavigation;
        bool m_visible = true;
        bool m_focused = false;
        bool m_comInitialized = false;
        EventRegistrationToken m_navigationStartingToken {};
        EventRegistrationToken m_sourceChangedToken {};
        EventRegistrationToken m_navigationCompletedToken {};
        EventRegistrationToken m_documentTitleChangedToken {};
        EventRegistrationToken m_newWindowRequestedToken {};
        EventRegistrationToken m_permissionRequestedToken {};
        EventRegistrationToken m_downloadStartingToken {};
    };
#endif

    std::unique_ptr<BrowserBackend> createPlatformBrowserBackend() {
#if defined(GEODE_IS_WINDOWS) && defined(GD_BROWSER_HAS_WEBVIEW2)
        return std::make_unique<WebView2BrowserBackend>();
#else
        return std::make_unique<ScaffoldBrowserBackend>(detectPlatformCapabilities());
#endif
    }
}
