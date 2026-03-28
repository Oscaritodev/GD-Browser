#pragma once

#include "BrowserModels.hpp"

#include <Geode/Geode.hpp>

#include <vector>

namespace gdbrowser {
    class SessionStore {
    public:
        void load(bool safeMode, bool restoreSession, std::string const& homepage);
        void save() const;

        std::vector<BrowserTabState> const& tabs() const;
        std::vector<BrowserHistoryEntry> const& history() const;
        std::vector<BrowserFavorite> const& favorites() const;
        size_t activeIndex() const;
        bool safeModeApplied() const;

        BrowserTabState const& activeTab() const;
        BrowserTabState& activeTab();
        void ensureAtLeastOneTab(std::string const& homepage);
        void addTab(std::string const& url, std::string const& title = {});
        void closeActiveTab(std::string const& homepage);
        void setActiveIndex(size_t index);
        void updateActiveTab(std::string const& url, std::string const& title = {});
        void setActiveTitle(std::string const& title);
        void recordHistory(std::string const& url, std::string const& title, size_t limit);
        bool isFavorite(std::string const& url) const;
        bool toggleFavorite(std::string const& url, std::string const& title);
        PermissionDecision decisionFor(std::string const& origin, BrowserPermissionKind kind) const;
        void setDecision(std::string const& origin, BrowserPermissionKind kind, PermissionDecision decision);
        void clearSiteData(std::string const& origin);

    private:
        static matjson::Value tabToJson(BrowserTabState const& tab);
        static BrowserTabState tabFromJson(matjson::Value const& value);
        static matjson::Value historyToJson(BrowserHistoryEntry const& entry);
        static BrowserHistoryEntry historyFromJson(matjson::Value const& value);
        static matjson::Value favoriteToJson(BrowserFavorite const& favorite);
        static BrowserFavorite favoriteFromJson(matjson::Value const& value);
        static matjson::Value permissionToJson(SitePermissionRule const& rule);
        static SitePermissionRule permissionFromJson(matjson::Value const& value);

        SitePermissionRule* findPermissionRule(std::string const& origin);
        SitePermissionRule const* findPermissionRule(std::string const& origin) const;

        std::vector<BrowserTabState> m_tabs;
        std::vector<BrowserHistoryEntry> m_history;
        std::vector<BrowserFavorite> m_favorites;
        std::vector<SitePermissionRule> m_permissions;
        size_t m_activeIndex = 0;
        bool m_safeModeApplied = false;
    };
}
