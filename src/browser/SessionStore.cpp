#include "SessionStore.hpp"

#include <algorithm>

namespace gdbrowser {
    namespace {
        constexpr auto kSessionSaveKey = "browser-session";
    }

    void SessionStore::load(bool safeMode, bool restoreSession, std::string const& homepage) {
        m_tabs.clear();
        m_history.clear();
        m_favorites.clear();
        m_permissions.clear();
        m_activeIndex = 0;
        m_safeModeApplied = safeMode;

        auto root = geode::Mod::get()->getSavedValue<matjson::Value>(kSessionSaveKey, matjson::Value::object());

        if (root["history"].isArray()) {
            for (auto const& item : root["history"]) {
                m_history.push_back(historyFromJson(item));
            }
        }

        if (root["favorites"].isArray()) {
            for (auto const& item : root["favorites"]) {
                m_favorites.push_back(favoriteFromJson(item));
            }
        }

        if (root["permissions"].isArray()) {
            for (auto const& item : root["permissions"]) {
                m_permissions.push_back(permissionFromJson(item));
            }
        }

        if (!safeMode && restoreSession && root["tabs"].isArray()) {
            for (auto const& item : root["tabs"]) {
                m_tabs.push_back(tabFromJson(item));
            }
            m_activeIndex = static_cast<size_t>(root["active-index"].asInt().unwrapOr(0));
        }

        this->ensureAtLeastOneTab(homepage);
        if (m_activeIndex >= m_tabs.size()) {
            m_activeIndex = 0;
        }
    }

    void SessionStore::save() const {
        std::vector<matjson::Value> tabs;
        std::vector<matjson::Value> history;
        std::vector<matjson::Value> favorites;
        std::vector<matjson::Value> permissions;

        tabs.reserve(m_tabs.size());
        history.reserve(m_history.size());
        favorites.reserve(m_favorites.size());
        permissions.reserve(m_permissions.size());

        for (auto const& tab : m_tabs) {
            tabs.push_back(tabToJson(tab));
        }
        for (auto const& item : m_history) {
            history.push_back(historyToJson(item));
        }
        for (auto const& item : m_favorites) {
            favorites.push_back(favoriteToJson(item));
        }
        for (auto const& item : m_permissions) {
            permissions.push_back(permissionToJson(item));
        }

        geode::Mod::get()->setSavedValue<matjson::Value>(
            kSessionSaveKey,
            matjson::makeObject({
                { "tabs", tabs },
                { "active-index", static_cast<int>(m_activeIndex) },
                { "history", history },
                { "favorites", favorites },
                { "permissions", permissions },
            })
        );
    }

    std::vector<BrowserTabState> const& SessionStore::tabs() const {
        return m_tabs;
    }

    std::vector<BrowserHistoryEntry> const& SessionStore::history() const {
        return m_history;
    }

    std::vector<BrowserFavorite> const& SessionStore::favorites() const {
        return m_favorites;
    }

    size_t SessionStore::activeIndex() const {
        return m_activeIndex;
    }

    bool SessionStore::safeModeApplied() const {
        return m_safeModeApplied;
    }

    BrowserTabState const& SessionStore::activeTab() const {
        return m_tabs.at(m_activeIndex);
    }

    BrowserTabState& SessionStore::activeTab() {
        return m_tabs.at(m_activeIndex);
    }

    void SessionStore::ensureAtLeastOneTab(std::string const& homepage) {
        if (!m_tabs.empty()) {
            return;
        }
        m_tabs.push_back({
            .id = makeOpaqueId("tab"),
            .url = homepage,
            .title = "Home",
            .restorable = true,
        });
        m_activeIndex = 0;
    }

    void SessionStore::addTab(std::string const& url, std::string const& title) {
        m_tabs.push_back({
            .id = makeOpaqueId("tab"),
            .url = url,
            .title = title.empty() ? "New Tab" : title,
            .restorable = true,
        });
        m_activeIndex = m_tabs.size() - 1;
        this->save();
    }

    void SessionStore::closeActiveTab(std::string const& homepage) {
        if (m_tabs.empty()) {
            this->ensureAtLeastOneTab(homepage);
            return;
        }

        m_tabs.erase(m_tabs.begin() + static_cast<std::ptrdiff_t>(m_activeIndex));
        if (m_activeIndex >= m_tabs.size() && !m_tabs.empty()) {
            m_activeIndex = m_tabs.size() - 1;
        }
        this->ensureAtLeastOneTab(homepage);
        this->save();
    }

    void SessionStore::setActiveIndex(size_t index) {
        if (index >= m_tabs.size()) {
            return;
        }
        m_activeIndex = index;
        this->save();
    }

    void SessionStore::updateActiveTab(std::string const& url, std::string const& title) {
        this->ensureAtLeastOneTab(url);
        auto& tab = this->activeTab();
        tab.url = url;
        if (!title.empty()) {
            tab.title = title;
        }
        this->save();
    }

    void SessionStore::setActiveTitle(std::string const& title) {
        if (m_tabs.empty()) {
            return;
        }
        this->activeTab().title = title;
        this->save();
    }

    void SessionStore::recordHistory(std::string const& url, std::string const& title, size_t limit) {
        if (url.empty()) {
            return;
        }

        m_history.erase(
            std::remove_if(
                m_history.begin(),
                m_history.end(),
                [&url](BrowserHistoryEntry const& entry) { return entry.url == url; }
            ),
            m_history.end()
        );

        m_history.insert(m_history.begin(), {
            .url = url,
            .title = title.empty() ? url : title,
            .visitedAt = unixNow(),
        });

        if (m_history.size() > limit) {
            m_history.resize(limit);
        }
        this->save();
    }

    bool SessionStore::isFavorite(std::string const& url) const {
        return std::any_of(
            m_favorites.begin(),
            m_favorites.end(),
            [&url](BrowserFavorite const& favorite) { return favorite.url == url; }
        );
    }

    bool SessionStore::toggleFavorite(std::string const& url, std::string const& title) {
        if (url.empty()) {
            return false;
        }

        auto found = std::find_if(
            m_favorites.begin(),
            m_favorites.end(),
            [&url](BrowserFavorite const& favorite) { return favorite.url == url; }
        );

        if (found != m_favorites.end()) {
            m_favorites.erase(found);
            this->save();
            return false;
        }

        m_favorites.insert(m_favorites.begin(), {
            .url = url,
            .title = title.empty() ? url : title,
            .addedAt = unixNow(),
        });
        this->save();
        return true;
    }

    PermissionDecision SessionStore::decisionFor(std::string const& origin, BrowserPermissionKind kind) const {
        if (origin.empty()) {
            return PermissionDecision::Ask;
        }
        auto* rule = this->findPermissionRule(origin);
        if (!rule) {
            return PermissionDecision::Ask;
        }

        switch (kind) {
            case BrowserPermissionKind::Audio: return rule->audio;
            case BrowserPermissionKind::Popups: return rule->popups;
            case BrowserPermissionKind::Clipboard: return rule->clipboard;
            case BrowserPermissionKind::ExternalProtocol: return rule->externalProtocol;
            case BrowserPermissionKind::FileAccess: return rule->fileAccess;
        }
        return PermissionDecision::Ask;
    }

    void SessionStore::setDecision(std::string const& origin, BrowserPermissionKind kind, PermissionDecision decision) {
        if (origin.empty()) {
            return;
        }

        auto* rule = this->findPermissionRule(origin);
        if (!rule) {
            m_permissions.push_back({ .origin = origin });
            rule = &m_permissions.back();
        }

        switch (kind) {
            case BrowserPermissionKind::Audio: rule->audio = decision; break;
            case BrowserPermissionKind::Popups: rule->popups = decision; break;
            case BrowserPermissionKind::Clipboard: rule->clipboard = decision; break;
            case BrowserPermissionKind::ExternalProtocol: rule->externalProtocol = decision; break;
            case BrowserPermissionKind::FileAccess: rule->fileAccess = decision; break;
        }
        this->save();
    }

    void SessionStore::clearSiteData(std::string const& origin) {
        if (origin.empty()) {
            return;
        }

        m_permissions.erase(
            std::remove_if(
                m_permissions.begin(),
                m_permissions.end(),
                [&origin](SitePermissionRule const& rule) { return rule.origin == origin; }
            ),
            m_permissions.end()
        );

        m_history.erase(
            std::remove_if(
                m_history.begin(),
                m_history.end(),
                [&origin](BrowserHistoryEntry const& entry) { return extractOrigin(entry.url) == origin; }
            ),
            m_history.end()
        );

        this->save();
    }

    matjson::Value SessionStore::tabToJson(BrowserTabState const& tab) {
        return matjson::makeObject({
            { "id", tab.id },
            { "url", tab.url },
            { "title", tab.title },
            { "restorable", tab.restorable },
        });
    }

    BrowserTabState SessionStore::tabFromJson(matjson::Value const& value) {
        return {
            .id = value["id"].asString().unwrapOr(makeOpaqueId("tab")),
            .url = value["url"].asString().unwrapOr("https://www.google.com"),
            .title = value["title"].asString().unwrapOr("Tab"),
            .restorable = value["restorable"].asBool().unwrapOr(true),
        };
    }

    matjson::Value SessionStore::historyToJson(BrowserHistoryEntry const& entry) {
        return matjson::makeObject({
            { "url", entry.url },
            { "title", entry.title },
            { "visited-at", entry.visitedAt },
        });
    }

    BrowserHistoryEntry SessionStore::historyFromJson(matjson::Value const& value) {
        return {
            .url = value["url"].asString().unwrapOr(""),
            .title = value["title"].asString().unwrapOr(""),
            .visitedAt = static_cast<int>(value["visited-at"].asInt().unwrapOr(0)),
        };
    }

    matjson::Value SessionStore::favoriteToJson(BrowserFavorite const& favorite) {
        return matjson::makeObject({
            { "url", favorite.url },
            { "title", favorite.title },
            { "added-at", favorite.addedAt },
        });
    }

    BrowserFavorite SessionStore::favoriteFromJson(matjson::Value const& value) {
        return {
            .url = value["url"].asString().unwrapOr(""),
            .title = value["title"].asString().unwrapOr(""),
            .addedAt = static_cast<int>(value["added-at"].asInt().unwrapOr(0)),
        };
    }

    matjson::Value SessionStore::permissionToJson(SitePermissionRule const& rule) {
        return matjson::makeObject({
            { "origin", rule.origin },
            { "audio", toString(rule.audio) },
            { "popups", toString(rule.popups) },
            { "clipboard", toString(rule.clipboard) },
            { "external", toString(rule.externalProtocol) },
            { "file", toString(rule.fileAccess) },
        });
    }

    SitePermissionRule SessionStore::permissionFromJson(matjson::Value const& value) {
        return {
            .origin = value["origin"].asString().unwrapOr(""),
            .audio = parsePermissionDecision(value["audio"].asString().unwrapOr("ask")),
            .popups = parsePermissionDecision(value["popups"].asString().unwrapOr("ask")),
            .clipboard = parsePermissionDecision(value["clipboard"].asString().unwrapOr("ask")),
            .externalProtocol = parsePermissionDecision(value["external"].asString().unwrapOr("ask")),
            .fileAccess = parsePermissionDecision(value["file"].asString().unwrapOr("ask")),
        };
    }

    SitePermissionRule* SessionStore::findPermissionRule(std::string const& origin) {
        auto found = std::find_if(
            m_permissions.begin(),
            m_permissions.end(),
            [&origin](SitePermissionRule const& rule) { return rule.origin == origin; }
        );
        return found == m_permissions.end() ? nullptr : &*found;
    }

    SitePermissionRule const* SessionStore::findPermissionRule(std::string const& origin) const {
        auto found = std::find_if(
            m_permissions.begin(),
            m_permissions.end(),
            [&origin](SitePermissionRule const& rule) { return rule.origin == origin; }
        );
        return found == m_permissions.end() ? nullptr : &*found;
    }
}
