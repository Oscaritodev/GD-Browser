#include "BrowserShell.hpp"

#include "BrowserController.hpp"

#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/TextArea.hpp>

#include <algorithm>
#include <array>
#include <optional>

namespace gdbrowser {
    namespace {
        std::string truncateUiText(std::string text, size_t maxLength) {
            if (text.size() <= maxLength) {
                return text;
            }
            return fmt::format("{}...", text.substr(0, maxLength - 3));
        }

        cocos2d::CCLayerColor* makeStrip(cocos2d::CCSize const& size, cocos2d::ccColor4B color) {
            return cocos2d::CCLayerColor::create(color, size.width, size.height);
        }

        cocos2d::CCLabelBMFont* makeLabel(
            std::string const& text,
            char const* font,
            float scale,
            cocos2d::ccColor3B color,
            cocos2d::CCPoint anchor = { 0.5f, 0.5f }
        ) {
            auto* label = cocos2d::CCLabelBMFont::create(text.c_str(), font);
            label->setScale(scale);
            label->setAnchorPoint(anchor);
            label->setColor(color);
            return label;
        }
    }

    BrowserShell* BrowserShell::create() {
        auto ret = new BrowserShell();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool BrowserShell::init() {
        if (!Popup::init(470.f, 330.f)) {
            return false;
        }

        this->setTitle("GD Browser");

        auto const size = m_mainLayer->getContentSize();

        auto* shellBackdrop = cocos2d::CCLayerColor::create(cocos2d::ccc4(245, 247, 250, 255), size.width - 18.f, size.height - 44.f);
        shellBackdrop->setPosition({ 9.f, 12.f });
        m_mainLayer->addChild(shellBackdrop, -3);

        auto* tabStrip = cocos2d::CCLayerColor::create(cocos2d::ccc4(231, 235, 240, 255), size.width - 18.f, 26.f);
        tabStrip->setPosition({ 9.f, size.height - 57.f });
        m_mainLayer->addChild(tabStrip, -2);

        auto* navStrip = cocos2d::CCLayerColor::create(cocos2d::ccc4(248, 249, 251, 255), size.width - 18.f, 34.f);
        navStrip->setPosition({ 9.f, size.height - 91.f });
        m_mainLayer->addChild(navStrip, -2);

        auto* chromeLogo = geode::createModLogo(geode::Mod::get());
        chromeLogo->setScale(0.5f);
        chromeLogo->setAnchorPoint({ 0.f, 0.5f });
        chromeLogo->setPosition({ 18.f, size.height - 44.f });
        m_mainLayer->addChild(chromeLogo);

        auto* brand = makeLabel("GD Browser", "goldFont.fnt", 0.48f, { 92, 98, 112 }, { 0.f, 0.5f });
        brand->setPosition({ 48.f, size.height - 44.f });
        m_mainLayer->addChild(brand);

        auto* activeTabCard = makeStrip({ 198.f, 22.f }, cocos2d::ccc4(255, 255, 255, 255));
        activeTabCard->setPosition({ 123.f, size.height - 55.f });
        m_mainLayer->addChild(activeTabCard);

        m_tabTitleLabel = makeLabel("New Tab", "bigFont.fnt", 0.37f, { 68, 73, 85 }, { 0.f, 0.5f });
        m_tabTitleLabel->setPosition({ 128.f, 11.f });
        activeTabCard->addChild(m_tabTitleLabel);

        m_tabLabel = makeLabel("1/1", "goldFont.fnt", 0.38f, { 118, 124, 136 }, { 1.f, 0.5f });
        m_tabLabel->setPosition({ 188.f, 11.f });
        activeTabCard->addChild(m_tabLabel);

        auto* omniboxBack = makeStrip({ 248.f, 24.f }, cocos2d::ccc4(233, 236, 240, 255));
        omniboxBack->setPosition({ 102.f, size.height - 86.f });
        m_mainLayer->addChild(omniboxBack);

        m_urlInput = geode::TextInput::create(232.f, "Search Google or type a URL", "bigFont.fnt");
        m_urlInput->setTextAlign(geode::TextInputAlign::Left);
        m_urlInput->hideBG();
        m_urlInput->setPosition({ 226.f, size.height - 74.f });
        m_urlInput->setCallback([this](std::string const& value) {
            this->onUrlChanged(value);
        });
        m_mainLayer->addChild(m_urlInput);

        auto* omniboxHint = makeLabel("Omnibox", "goldFont.fnt", 0.32f, { 124, 130, 142 }, { 0.f, 0.5f });
        omniboxHint->setPosition({ 106.f, size.height - 74.f });
        m_mainLayer->addChild(omniboxHint);

        auto* menu = cocos2d::CCMenu::create();
        menu->setPosition(cocos2d::CCPointZero);
        m_mainLayer->addChild(menu, 2);

        struct ToolbarButtonSpec {
            char const* label;
            float width;
            cocos2d::SEL_MenuHandler handler;
            cocos2d::CCPoint position;
            bool accent = false;
        };

        std::array<ToolbarButtonSpec, 12> buttons {{
            { "<", 26.f, menu_selector(BrowserShell::onBack), { 28.f, size.height - 74.f } },
            { ">", 26.f, menu_selector(BrowserShell::onForward), { 58.f, size.height - 74.f } },
            { "R", 30.f, menu_selector(BrowserShell::onReload), { 88.f, size.height - 74.f } },
            { "H", 30.f, menu_selector(BrowserShell::onHome), { 362.f, size.height - 74.f } },
            { "Go", 44.f, menu_selector(BrowserShell::onGo), { 405.f, size.height - 74.f }, true },
            { "+", 26.f, menu_selector(BrowserShell::onNewTab), { 412.f, size.height - 44.f } },
            { "x", 26.f, menu_selector(BrowserShell::onCloseTab), { 442.f, size.height - 44.f } },
            { "*", 26.f, menu_selector(BrowserShell::onFavorite), { 430.f, 50.f } },
            { "Up", 34.f, menu_selector(BrowserShell::onUpload), { 390.f, 50.f } },
            { "Dl", 34.f, menu_selector(BrowserShell::onDownloads), { 348.f, 50.f } },
            { "Out", 38.f, menu_selector(BrowserShell::onExternal), { 303.f, 50.f } },
            { "Cfg", 38.f, menu_selector(BrowserShell::onSettings), { 258.f, 50.f } },
        }};

        for (auto const& spec : buttons) {
            auto* item = this->makeToolbarButton(spec.label, spec.width, spec.handler, spec.accent);
            item->setPosition(spec.position);
            menu->addChild(item);
        }

        auto* clearButton = this->makeToolbarButton("Clear", 48.f, menu_selector(BrowserShell::onClearSiteData));
        clearButton->setPosition({ 206.f, 50.f });
        menu->addChild(clearButton);

        auto* keysButton = this->makeToolbarButton("Keys", 46.f, menu_selector(BrowserShell::onKeybinds));
        keysButton->setPosition({ 152.f, 50.f });
        menu->addChild(keysButton);

        auto* prevTabButton = this->makeToolbarButton("Tab<", 42.f, menu_selector(BrowserShell::onPrevTab));
        prevTabButton->setPosition({ 93.f, 50.f });
        menu->addChild(prevTabButton);

        auto* nextTabButton = this->makeToolbarButton("Tab>", 42.f, menu_selector(BrowserShell::onNextTab));
        nextTabButton->setPosition({ 43.f, 50.f });
        menu->addChild(nextTabButton);

        auto* viewportBorder = cocos2d::CCLayerColor::create(cocos2d::ccc4(216, 220, 228, 255), size.width - 34.f, 181.f);
        viewportBorder->setPosition({ 17.f, 67.f });
        m_mainLayer->addChild(viewportBorder, -1);

        m_viewport = cocos2d::CCLayerColor::create(cocos2d::ccc4(255, 255, 255, 255), size.width - 38.f, 177.f);
        m_viewport->setPosition({ 19.f, 69.f });
        m_mainLayer->addChild(m_viewport);

        m_scaffoldOverlay = cocos2d::CCNode::create();
        m_viewport->addChild(m_scaffoldOverlay);

        auto const viewSize = m_viewport->getContentSize();

        auto* hero = cocos2d::CCSprite::create(geode::Mod::get()->expandSpriteName("browser-hero.png").c_str());
        if (!hero) {
            hero = cocos2d::CCSprite::create("browser-hero.png");
        }
        if (hero) {
            hero->setPosition({ viewSize.width - 84.f, viewSize.height / 2.f + 4.f });
            hero->setScale(std::min(0.48f, 96.f / hero->getContentSize().width));
            m_scaffoldOverlay->addChild(hero);
        } else {
            auto* miniBrowser = cocos2d::CCLayerColor::create(cocos2d::ccc4(245, 247, 251, 255), 146.f, 92.f);
            miniBrowser->setPosition({ viewSize.width - 168.f, 38.f });
            m_scaffoldOverlay->addChild(miniBrowser);

            auto* miniTop = cocos2d::CCLayerColor::create(cocos2d::ccc4(233, 237, 243, 255), 146.f, 18.f);
            miniTop->setPosition({ 0.f, 74.f });
            miniBrowser->addChild(miniTop);

            auto* miniDotA = cocos2d::CCLayerColor::create(cocos2d::ccc4(218, 223, 231, 255), 10.f, 10.f);
            miniDotA->setPosition({ 10.f, 40.f });
            miniBrowser->addChild(miniDotA);

            auto* miniDotB = cocos2d::CCLayerColor::create(cocos2d::ccc4(218, 223, 231, 255), 10.f, 10.f);
            miniDotB->setPosition({ 24.f, 40.f });
            miniBrowser->addChild(miniDotB);

            auto* miniOmnibox = cocos2d::CCLayerColor::create(cocos2d::ccc4(228, 234, 242, 255), 76.f, 14.f);
            miniOmnibox->setPosition({ 40.f, 38.f });
            miniBrowser->addChild(miniOmnibox);

            auto* miniGo = cocos2d::CCLayerColor::create(cocos2d::ccc4(66, 133, 244, 255), 24.f, 14.f);
            miniGo->setPosition({ 118.f, 38.f });
            miniBrowser->addChild(miniGo);
        }

        auto* overlayLogo = geode::createModLogo(geode::Mod::get());
        overlayLogo->setScale(0.82f);
        overlayLogo->setPosition({ 56.f, viewSize.height - 46.f });
        m_scaffoldOverlay->addChild(overlayLogo);

        auto* overlayTitle = makeLabel("Search or type a URL", "goldFont.fnt", 0.65f, { 58, 66, 82 }, { 0.f, 0.5f });
        overlayTitle->setPosition({ 110.f, viewSize.height - 44.f });
        m_scaffoldOverlay->addChild(overlayTitle);

        auto* overlayHint = geode::SimpleTextArea::create(
            "Write a URL like google.com or a search like geode sdk. If WebView2 is available, pages render here inside Geometry Dash.",
            "chatFont.fnt",
            0.7f,
            248.f
        );
        overlayHint->setAlignment(cocos2d::kCCTextAlignmentLeft);
        overlayHint->setColor(cocos2d::ccc4(104, 111, 125, 255));
        overlayHint->setAnchorPoint({ 0.f, 1.f });
        overlayHint->setPosition({ 24.f, viewSize.height - 72.f });
        m_scaffoldOverlay->addChild(overlayHint);

        auto* quickLinksLabel = makeLabel("Quick open", "goldFont.fnt", 0.42f, { 126, 133, 145 }, { 0.f, 0.5f });
        quickLinksLabel->setPosition({ 24.f, 46.f });
        m_scaffoldOverlay->addChild(quickLinksLabel);

        auto* quickMenu = cocos2d::CCMenu::create();
        quickMenu->setPosition(cocos2d::CCPointZero);
        m_scaffoldOverlay->addChild(quickMenu);

        auto* googleButton = this->makeToolbarButton("Google", 74.f, menu_selector(BrowserShell::onQuickGoogle), true);
        googleButton->setPosition({ 58.f, 22.f });
        quickMenu->addChild(googleButton);

        auto* youtubeButton = this->makeToolbarButton("YouTube", 82.f, menu_selector(BrowserShell::onQuickYouTube));
        youtubeButton->setPosition({ 148.f, 22.f });
        quickMenu->addChild(youtubeButton);

        auto* docsButton = this->makeToolbarButton("Geode Docs", 96.f, menu_selector(BrowserShell::onQuickDocs));
        docsButton->setPosition({ 252.f, 22.f });
        quickMenu->addChild(docsButton);

        auto* statusStrip = cocos2d::CCLayerColor::create(cocos2d::ccc4(248, 249, 251, 255), size.width - 18.f, 26.f);
        statusStrip->setPosition({ 9.f, 16.f });
        m_mainLayer->addChild(statusStrip, -2);

        m_statusTitleLabel = makeLabel("Ready to browse", "goldFont.fnt", 0.42f, { 71, 78, 91 }, { 0.f, 0.5f });
        m_statusTitleLabel->setPosition({ 18.f, 29.f });
        m_mainLayer->addChild(m_statusTitleLabel);

        m_statusLabel = geode::SimpleTextArea::create("Type a URL or a search query.", "chatFont.fnt", 0.72f, 250.f);
        m_statusLabel->setAlignment(cocos2d::kCCTextAlignmentLeft);
        m_statusLabel->setColor(cocos2d::ccc4(106, 114, 128, 255));
        m_statusLabel->setAnchorPoint({ 0.f, 0.5f });
        m_statusLabel->setPosition({ 150.f, 28.f });
        m_mainLayer->addChild(m_statusLabel);

        BrowserController::get().attachView(
            m_viewport,
            {
                .x = m_viewport->getPositionX(),
                .y = m_viewport->getPositionY(),
                .width = m_viewport->getContentSize().width,
                .height = m_viewport->getContentSize().height,
            }
        );

        this->syncAddressBar();
        this->schedule(schedule_selector(BrowserShell::onRefreshTick), 0.25f);
        this->schedule(schedule_selector(BrowserShell::onWatchdogTick), 1.f);
        this->rebuildStatus();
        m_urlInput->focus();
        return true;
    }

    void BrowserShell::onClose(cocos2d::CCObject* sender) {
        BrowserController::get().detachView();
        Popup::onClose(sender);
    }

    void BrowserShell::syncAddressBar() {
        m_pendingUrl = BrowserController::get().snapshot().currentUrl;
        if (m_urlInput) {
            m_urlInput->setString(m_pendingUrl, false);
        }
    }

    void BrowserShell::rebuildStatus() {
        auto snapshot = BrowserController::get().snapshot();
        auto title = snapshot.currentTitle.empty() ? snapshot.currentUrl : snapshot.currentTitle;
        if (title.empty()) {
            title = "New Tab";
        }

        m_tabTitleLabel->setString(truncateUiText(title, 28).c_str());
        m_tabLabel->setString(fmt::format("{}/{}", snapshot.tabs.empty() ? 0 : snapshot.activeTabIndex + 1, snapshot.tabs.size()).c_str());

        std::string footerTitle = snapshot.loading ? "Loading" : "Ready";
        std::string footerStatus;
        cocos2d::ccColor4B footerColor = cocos2d::ccc4(106, 114, 128, 255);

        if (!snapshot.lastError.empty()) {
            footerTitle = "Problem";
            footerStatus = snapshot.lastError;
            footerColor = cocos2d::ccc4(196, 62, 62, 255);
        } else if (snapshot.loading) {
            footerStatus = snapshot.currentUrl.empty() ? "Loading page..." : truncateUiText(snapshot.currentUrl, 80);
        } else if (!snapshot.capabilities.nativeBridgeReady) {
            footerTitle = "Preview Mode";
            footerStatus = "Searches, tabs and recovery are live. The native renderer is still connecting or running in scaffold mode.";
            footerColor = cocos2d::ccc4(113, 94, 46, 255);
        } else if (!snapshot.statusLine.empty()) {
            footerStatus = snapshot.statusLine;
        } else {
            footerStatus = snapshot.currentUrl.empty() ? "Type a URL or a search query." : truncateUiText(snapshot.currentUrl, 80);
        }

        m_statusTitleLabel->setString(footerTitle.c_str());
        m_statusLabel->setText(footerStatus);
        m_statusLabel->setColor(footerColor);
        m_scaffoldOverlay->setVisible(!snapshot.capabilities.nativeBridgeReady);
    }

    CCMenuItemSpriteExtra* BrowserShell::makeToolbarButton(
        std::string const& text,
        float width,
        cocos2d::SEL_MenuHandler handler,
        bool accent
    ) {
        auto* node = cocos2d::CCNode::create();
        node->setContentSize({ width, 22.f });

        auto* background = makeStrip(
            { width, 22.f },
            accent ? cocos2d::ccc4(66, 133, 244, 255) : cocos2d::ccc4(232, 236, 241, 255)
        );
        background->setPosition({ 0.f, 0.f });
        node->addChild(background);

        auto* label = makeLabel(
            text,
            accent ? "goldFont.fnt" : "bigFont.fnt",
            0.4f,
            accent ? cocos2d::ccColor3B { 255, 255, 255 } : cocos2d::ccColor3B { 67, 73, 87 }
        );
        label->setPosition({ width / 2.f, 11.f });
        node->addChild(label);

        auto* selectedNode = cocos2d::CCNode::create();
        selectedNode->setContentSize({ width, 22.f });

        auto* selectedBackground = makeStrip(
            { width, 22.f },
            accent ? cocos2d::ccc4(43, 108, 214, 255) : cocos2d::ccc4(215, 220, 227, 255)
        );
        selectedBackground->setPosition({ 0.f, 0.f });
        selectedNode->addChild(selectedBackground);

        auto* selectedLabel = makeLabel(
            text,
            accent ? "goldFont.fnt" : "bigFont.fnt",
            0.4f,
            accent ? cocos2d::ccColor3B { 255, 255, 255 } : cocos2d::ccColor3B { 52, 58, 70 }
        );
        selectedLabel->setPosition({ width / 2.f, 11.f });
        selectedNode->addChild(selectedLabel);

        auto* button = CCMenuItemSpriteExtra::create(node, selectedNode, this, handler);
        button->setSizeMult(1.05f);
        return button;
    }

    void BrowserShell::onUrlChanged(std::string const& value) {
        m_pendingUrl = value;
    }

    void BrowserShell::onGo(cocos2d::CCObject*) {
        BrowserController::get().navigate(m_pendingUrl);
        this->syncAddressBar();
        this->rebuildStatus();
    }

    void BrowserShell::onBack(cocos2d::CCObject*) {
        BrowserController::get().goBack();
        this->syncAddressBar();
        this->rebuildStatus();
    }

    void BrowserShell::onForward(cocos2d::CCObject*) {
        BrowserController::get().goForward();
        this->syncAddressBar();
        this->rebuildStatus();
    }

    void BrowserShell::onReload(cocos2d::CCObject*) {
        BrowserController::get().reload();
        this->syncAddressBar();
        this->rebuildStatus();
    }

    void BrowserShell::onHome(cocos2d::CCObject*) {
        BrowserController::get().goHome();
        this->syncAddressBar();
        this->rebuildStatus();
    }

    void BrowserShell::onNewTab(cocos2d::CCObject*) {
        BrowserController::get().newTab(m_pendingUrl);
        this->syncAddressBar();
        this->rebuildStatus();
    }

    void BrowserShell::onCloseTab(cocos2d::CCObject*) {
        BrowserController::get().closeTab();
        this->syncAddressBar();
        this->rebuildStatus();
    }

    void BrowserShell::onPrevTab(cocos2d::CCObject*) {
        BrowserController::get().selectNextTab(-1);
        this->syncAddressBar();
        this->rebuildStatus();
    }

    void BrowserShell::onNextTab(cocos2d::CCObject*) {
        BrowserController::get().selectNextTab(1);
        this->syncAddressBar();
        this->rebuildStatus();
    }

    void BrowserShell::onFavorite(cocos2d::CCObject*) {
        BrowserController::get().toggleFavorite();
        this->rebuildStatus();
    }

    void BrowserShell::onUpload(cocos2d::CCObject*) {
        BrowserController::get().requestUpload();
        this->rebuildStatus();
    }

    void BrowserShell::onDownloads(cocos2d::CCObject*) {
        BrowserController::get().openDownloadsFolder();
        this->rebuildStatus();
    }

    void BrowserShell::onExternal(cocos2d::CCObject*) {
        BrowserController::get().openExternally();
        this->rebuildStatus();
    }

    void BrowserShell::onClearSiteData(cocos2d::CCObject*) {
        geode::createQuickPopup(
            "Clear Site Data",
            "Remove stored history and permission decisions for the current origin?",
            "Cancel",
            "Clear",
            250.f,
            [](FLAlertLayer*, bool confirmed) {
                if (!confirmed) {
                    return;
                }
                BrowserController::get().clearSiteDataForActiveOrigin();
            }
        );
    }

    void BrowserShell::onSettings(cocos2d::CCObject*) {
        geode::openSettingsPopup(geode::Mod::get(), false);
    }

    void BrowserShell::onKeybinds(cocos2d::CCObject*) {
        geode::openKeybindsPopup(std::nullopt, geode::Mod::get());
    }

    void BrowserShell::onQuickGoogle(cocos2d::CCObject*) {
        BrowserController::get().navigate("google");
        this->syncAddressBar();
        this->rebuildStatus();
    }

    void BrowserShell::onQuickYouTube(cocos2d::CCObject*) {
        BrowserController::get().navigate("youtube");
        this->syncAddressBar();
        this->rebuildStatus();
    }

    void BrowserShell::onQuickDocs(cocos2d::CCObject*) {
        BrowserController::get().navigate("docs.geode-sdk.org");
        this->syncAddressBar();
        this->rebuildStatus();
    }

    void BrowserShell::onRefreshTick(float) {
        BrowserController::get().updateBounds({
            .x = m_viewport->getPositionX(),
            .y = m_viewport->getPositionY(),
            .width = m_viewport->getContentSize().width,
            .height = m_viewport->getContentSize().height,
        });
        this->rebuildStatus();
    }

    void BrowserShell::onWatchdogTick(float) {
        BrowserController::get().tickWatchdog();
    }
}
