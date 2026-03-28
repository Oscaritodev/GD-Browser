#include <Geode/Geode.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "browser/BrowserShell.hpp"

using namespace geode::prelude;

namespace {
    void showBrowser() {
        if (auto* popup = gdbrowser::BrowserShell::create()) {
            popup->show();
        }
    }
}

$on_game(Loaded) {
    listenForKeybindSettingPresses(
        "open-browser-keybind",
        [](Keybind const&, bool down, bool repeat, double) {
            if (!down || repeat) {
                return;
            }
            showBrowser();
        },
        Mod::get()
    );
}

class $modify(GDBrowserMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }

        auto buttonLabel = CCLabelBMFont::create("Browser", "goldFont.fnt");
        buttonLabel->setScale(0.5f);
        auto button = CCMenuItemLabel::create(buttonLabel, this, menu_selector(GDBrowserMenuLayer::onOpenBrowser));
        button->setID("open-browser-button"_spr);

        if (auto* bottomMenu = typeinfo_cast<CCMenu*>(this->getChildByID("bottom-menu"))) {
            bottomMenu->addChild(button);
            bottomMenu->updateLayout();
        } else {
            auto* fallbackMenu = CCMenu::create();
            fallbackMenu->setPosition({ CCDirector::sharedDirector()->getWinSize().width - 55.f, 40.f });
            fallbackMenu->addChild(button);
            this->addChild(fallbackMenu, 999);
        }

        return true;
    }

    void onOpenBrowser(CCObject*) {
        showBrowser();
    }
};
