#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/GJDropDownLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/modify/UILayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/SimpleAxisLayout.hpp>
#include <Geode/utils/cocos.hpp>
#include <Geode/utils/Keyboard.hpp>

using namespace geode::prelude;

static void addBindSprites(CCNodeRGBA* target, const char* action) {
    if (target == nullptr) return;
    target->removeAllChildren();

    auto bindContainer = CCNode::create();
    bool first = true;
    for (auto& bind : Mod::get()->getSettingValue<std::vector<Keybind>>(action)) {
        if (!first) {
            auto separator = CCLabelBMFont::create("/", "bigFont.fnt");
            separator->setScale(.8f);
            separator->setOpacity(target->getOpacity());
            bindContainer->addChild(separator);
        }
        first = false;
        auto label = bind.createNode();
        if (auto text = typeinfo_cast<CCLabelBMFont*>(label)) {
            text->setFntFile("bigFont.fnt");
        }
        label->setScale(.8f);
        if (auto rgba = typeinfo_cast<CCRGBAProtocol*>(label)) {
            rgba->setOpacity(target->getOpacity());
        }
        bindContainer->addChild(label);
    }
    bindContainer->setID("binds"_spr);
    bindContainer->setContentSize({
        target->getScaledContentWidth(), 40.f
    });
    bindContainer->setLayout(SimpleRowLayout::create()->setMainAxisScaling(AxisScaling::ScaleDown));
    bindContainer->setAnchorPoint({ .5f, .5f });
    bindContainer->setPosition(target->getContentWidth() / 2, -1.f);
    target->addChild(bindContainer);
}

struct $modify(PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        this->addEventListener(
            KeybindSettingPressedEventV3(Mod::get(), "unpause-level"),
            [this](Keybind const& keybind, bool down, bool repeat, double timestamp) {
                auto ref = Ref(this);

                if (repeat || !down) {
                    return ListenerResult::Propagate;
                }

                // Remove any popups (looking at you, confirm exit)
                CCScene* active = CCDirector::sharedDirector()->getRunningScene();
                if (auto alert = active->getChildByType<FLAlertLayer>(0)) {
                    return ListenerResult::Propagate;
                }
                this->onResume(nullptr);

                return ListenerResult::Stop;
            }
        );

        this->addEventListener(
            KeybindSettingPressedEventV3(Mod::get(), "exit-level"),
            [this](Keybind const& keybind, bool down, bool repeat, double timestamp) {
                auto ref = Ref(this);

                if (!repeat && down) {
                    this->onQuit(nullptr);
                    return ListenerResult::Stop;
                }
                return ListenerResult::Propagate;
            }
        );

        this->addEventListener(
            KeybindSettingPressedEventV3(Mod::get(), "practice-level"),
            [this](Keybind const& keybind, bool down, bool repeat, double timestamp) {
                auto ref = Ref(this);

                if (!repeat && down) {
                    if(PlayLayer::get() && PlayLayer::get()->m_isPracticeMode) {
                        this->onNormalMode(nullptr);
                    } else {
                        this->onPracticeMode(nullptr);
                    }
                    return ListenerResult::Stop;
                }
                return ListenerResult::Propagate;
            }
        );
    }

    void keyDown(enumKeyCodes key, double timestamp) {
        if (key == enumKeyCodes::KEY_Escape) {
            PauseLayer::keyDown(key, timestamp);
        }
    }
};

struct $modify(UILayer) {
    static inline int platformButton() {
        return 1;
    }

    bool isPaused() {
        return !this->isCurrentPlayLayer()
            || PlayLayer::get()->getParent()->getChildByType<PauseLayer>(0) != nullptr
            || PlayLayer::get()->getChildByType<GJDropDownLayer>(0) != nullptr;
    }

    bool isEditorPlaytest() {
        auto lel = LevelEditorLayer::get();
        if (!lel) return false;
        return lel->m_playbackMode == PlaybackMode::Playing;
    }

    bool isCurrentPlayLayer() {
        auto playLayer = CCScene::get()->getChildByType<PlayLayer>(0);
        return playLayer != nullptr && playLayer == PlayLayer::get() && playLayer->getChildByType<UILayer>(0) == this;
    }

    void pressKeyFallthrough(enumKeyCodes key, bool down, double timestamp) {
        auto dispatcher = CCDirector::get()->getKeyboardDispatcher();

        auto shift = dispatcher->getShiftKeyPressed();
        auto ctrl = dispatcher->getControlKeyPressed();
        auto alt = dispatcher->getAltKeyPressed();
        auto cmd = dispatcher->getCommandKeyPressed();

        pressKeyFallthrough(key, down, shift, ctrl, alt, cmd, timestamp);
    }

    void pressKeyFallthrough(enumKeyCodes key, bool down, bool shift, bool ctrl, bool alt, bool cmd, double timestamp) {
        // amazing hack
        if (this->isPaused())
            return;

        // enable quick keys - this mildly diverts from vanilla behavior
        // because some keybinds are only supposed to work with that option enabled
        // however we do provide our own way to disable them
        // so requiring them both to be enabled just ended up confusing users
        auto GM = GameManager::sharedState();
        auto quickKeys = GM->getGameVariable("0163");
        GM->setGameVariable("0163", true);

        auto dispatcher = CCDirector::get()->getKeyboardDispatcher();

        auto oShift = dispatcher->getShiftKeyPressed();
        auto oCtrl = dispatcher->getControlKeyPressed();
        auto oAlt = dispatcher->getAltKeyPressed();
        auto oCmd = dispatcher->getCommandKeyPressed();

        dispatcher->updateModifierKeys(shift, ctrl, alt, cmd);

        if (down) {
            this->keyDown(key, timestamp);
        } else {
            this->keyUp(key, timestamp);
        }
        dispatcher->updateModifierKeys(oShift, oCtrl, oAlt, oCmd);

        //disable quick keys to restore previous state
        GM->setGameVariable("0163", quickKeys);
    }

    bool init(GJBaseGameLayer* layer) {
        if (!UILayer::init(layer))
            return false;

        // Geode 6 dispatches physical keyboard input through this event before
        // forwarding it to the Cocos2d keyboard dispatcher. Run after Geode's
        // keybind listener so configured binds can consume their events first.
        this->addEventListener(
            KeyboardInputEvent(),
            [this](KeyboardInputData& data) {
                if (data.key == enumKeyCodes::KEY_Escape) {
                    return ListenerResult::Propagate;
                }

                auto handlers = CCKeyboardDispatcher::get()->m_pHandlers;
                if (!handlers) {
                    return ListenerResult::Propagate;
                }

                auto handler = static_cast<CCKeyboardHandler*>(handlers->lastObject());
                if (!handler || handler->getDelegate() != static_cast<CCKeyboardDelegate*>(static_cast<CCLayer*>(this))) {
                    return ListenerResult::Propagate;
                }

                // Preserve the old handleKeypress behavior: swallow ordinary
                // game input unless a Custom Keybind handled it first.
                return ListenerResult::Stop;
            },
            1000
        );

        // delay by a single frame
        geode::Loader::get()->queueInMainThread([this, self = Ref(this)] {

            this->defineKeybind("jump-p1", [this](bool down, bool repeat, double timestamp) {
                if (repeat || (this->isPaused() && !this->isEditorPlaytest())) {
                    return ListenerResult::Propagate;
                }

                // commenting this out might technically be a cheat but ppl are haters
                /*if ((down && m_p1Jumping) || (!down && !m_p1Jumping)) {
                    return ListenerResult::Stop;
                }*/

                PlayLayer::get()->queueButton(static_cast<int>(PlayerButton::Jump), down, false, timestamp);
                if (down) {
                    m_p1Jumping = true;
                } else {
                    m_p1Jumping = false;
                }
                return ListenerResult::Stop;
            });
            this->defineKeybind("jump-p2", [this](bool down, bool repeat, double timestamp) {
                if (repeat || (this->isPaused() && !this->isEditorPlaytest())) {
                    return ListenerResult::Propagate;
                }

                // commenting this out might technically be a cheat but ppl are haters
                /*if ((down && m_p2Jumping) || (!down && !m_p2Jumping)) {
                    return ListenerResult::Stop;
                }*/

                PlayLayer::get()->queueButton(static_cast<int>(PlayerButton::Jump), down, true, timestamp);
                if (down) {
                    m_p2Jumping = true;
                } else {
                    m_p2Jumping = false;
                }
                return ListenerResult::Stop;
            });

            // do not do anything in the editor
            if (!PlayLayer::get()) return;
            
            this->defineKeybind("place-checkpoint", [this](bool down, bool repeat, double timestamp) {
                if (repeat || this->isPaused()) {
                    return ListenerResult::Propagate;
                }
                if (down && PlayLayer::get()->m_isPracticeMode) {
                    PlayLayer::get()->markCheckpoint();
                }
                return ListenerResult::Stop;
            });
            this->defineKeybind("delete-checkpoint", [this](bool down, bool repeat, double timestamp) {
                if (repeat || this->isPaused()) {
                    return ListenerResult::Propagate;
                }
                if (down && PlayLayer::get()->m_isPracticeMode) {
                    // false removes the newest checkpoint in Geometry Dash 2.2091.
                    PlayLayer::get()->removeCheckpoint(false);
                }
                return ListenerResult::Stop;
            });
            this->defineKeybind("pause-level", [this](bool down, bool repeat, double timestamp) {
                if (!repeat && down && this->isCurrentPlayLayer() && !this->isPaused()) {
                    PlayLayer::get()->pauseGame(true);
                    return ListenerResult::Stop;
                }
                return ListenerResult::Propagate;
            });
            this->defineKeybind("restart-level", [this](bool down, bool repeat, double timestamp) {
                if (repeat || this->isPaused() || !this->isCurrentPlayLayer()) {
                    return ListenerResult::Propagate;
                }
                if (down) {
                    PlayLayer::get()->resetLevel();
                }
                return ListenerResult::Stop;
            });
            this->defineKeybind("full-restart-level", [this](bool down, bool repeat, double timestamp) {
                if (repeat || this->isPaused() || !this->isCurrentPlayLayer()) {
                    return ListenerResult::Propagate;
                }
                this->pressKeyFallthrough(KEY_R, down, false, true, false, false, timestamp);
                return ListenerResult::Stop;
            });
            this->defineKeybind("move-left-p1", [this](bool down, bool repeat, double timestamp) {
                if (repeat || this->isPaused()) {
                    return ListenerResult::Propagate;
                }
                PlayLayer::get()->queueButton(static_cast<int>(PlayerButton::Left), down, false, timestamp);
                return ListenerResult::Stop;
            });
            this->defineKeybind("move-right-p1", [this](bool down, bool repeat, double timestamp) {
                if (repeat || this->isPaused()) {
                    return ListenerResult::Propagate;
                }
                PlayLayer::get()->queueButton(static_cast<int>(PlayerButton::Right), down, false, timestamp);
                return ListenerResult::Stop;
            });
            this->defineKeybind("move-left-p2", [this](bool down, bool repeat, double timestamp) {
                if (repeat || this->isPaused()) {
                    return ListenerResult::Propagate;
                }
                PlayLayer::get()->queueButton(static_cast<int>(PlayerButton::Left), down, true, timestamp);
                return ListenerResult::Stop;
            });
            this->defineKeybind("move-right-p2", [this](bool down, bool repeat, double timestamp) {
                if (repeat || this->isPaused()) {
                    return ListenerResult::Propagate;
                }
                PlayLayer::get()->queueButton(static_cast<int>(PlayerButton::Right), down, true, timestamp);
                return ListenerResult::Stop;
            });
            this->defineKeybind("toggle-hitboxes", [this](bool down, bool repeat, double timestamp) {
                if (!repeat && down && this->isCurrentPlayLayer() && !this->isPaused()) {
                    auto gameManager = GameManager::sharedState();
                    gameManager->setGameVariable("0166", !gameManager->getGameVariable("0166"));
                    return ListenerResult::Stop;
                }
                return ListenerResult::Propagate;
            });
            // display practice mode button keybinds
            if (auto menu = this->getChildByID("checkpoint-menu")) {
                if (auto add = typeinfo_cast<CCMenuItemSpriteExtra*>(menu->getChildByID("add-checkpoint-button"))) {
                    addBindSprites(
                        typeinfo_cast<CCNodeRGBA*>(add->getNormalImage()),
                        "place-checkpoint"
                    );
                }
                if (auto rem = typeinfo_cast<CCMenuItemSpriteExtra*>(menu->getChildByID("remove-checkpoint-button"))) {
                    addBindSprites(
                        typeinfo_cast<CCNodeRGBA*>(rem->getNormalImage()),
                        "delete-checkpoint"
                    );
                }
            }
        });

        return true;
    }

    void defineKeybind(std::string id, CopyableFunction<bool(bool, bool, double)> callback) {
        // adding the events to playlayer instead
        GJBaseGameLayer::get()->addEventListener(
            KeybindSettingPressedEventV3(Mod::get(), std::move(id)),
            [callback = std::move(callback)](Keybind const& keybind, bool down, bool repeat, double timestamp) {
                return callback(down, repeat, timestamp);
            }
        );
    }

};
