#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

namespace geode {
    class SimpleTextArea;
}
class CCMenuItemSpriteExtra;

namespace gdbrowser {
    class BrowserShell final : public geode::Popup {
    public:
        static BrowserShell* create();
        void onClose(cocos2d::CCObject* sender) override;

    protected:
        bool init();

    private:
        void syncAddressBar();
        void rebuildStatus();
        CCMenuItemSpriteExtra* makeToolbarButton(std::string const& text, float width, cocos2d::SEL_MenuHandler handler, bool accent = false);
        void onUrlChanged(std::string const& value);
        void onGo(cocos2d::CCObject*);
        void onBack(cocos2d::CCObject*);
        void onForward(cocos2d::CCObject*);
        void onReload(cocos2d::CCObject*);
        void onHome(cocos2d::CCObject*);
        void onNewTab(cocos2d::CCObject*);
        void onCloseTab(cocos2d::CCObject*);
        void onPrevTab(cocos2d::CCObject*);
        void onNextTab(cocos2d::CCObject*);
        void onFavorite(cocos2d::CCObject*);
        void onUpload(cocos2d::CCObject*);
        void onDownloads(cocos2d::CCObject*);
        void onExternal(cocos2d::CCObject*);
        void onClearSiteData(cocos2d::CCObject*);
        void onSettings(cocos2d::CCObject*);
        void onKeybinds(cocos2d::CCObject*);
        void onQuickGoogle(cocos2d::CCObject*);
        void onQuickYouTube(cocos2d::CCObject*);
        void onQuickDocs(cocos2d::CCObject*);
        void onRefreshTick(float);
        void onWatchdogTick(float);

        geode::TextInput* m_urlInput = nullptr;
        cocos2d::CCLayerColor* m_viewport = nullptr;
        cocos2d::CCLabelBMFont* m_statusTitleLabel = nullptr;
        geode::SimpleTextArea* m_statusLabel = nullptr;
        cocos2d::CCLabelBMFont* m_tabTitleLabel = nullptr;
        cocos2d::CCLabelBMFont* m_tabLabel = nullptr;
        cocos2d::CCNode* m_scaffoldOverlay = nullptr;
        std::string m_pendingUrl;
    };
}
