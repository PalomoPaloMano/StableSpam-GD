#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/UILayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <chrono>
#include <deque>

using namespace geode::prelude;

// ===================== ESTADOS GLOBALES =====================
static bool g_ckOn           = true;
static bool g_enableSpam     = true;
static bool g_enableStraight = true;
static bool g_enableWave     = true;
static bool g_enableSave     = false;

static double g_lastSaveTime = 0.0;
static const double SAVE_COOLDOWN = 1.5;

static double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// ===================== PANEL CK =====================
class CKPanel : public CCLayer {
public:
    static CKPanel* create() {
        auto ret = new CKPanel();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;

        this->setTouchEnabled(true);
        this->setKeypadEnabled(true);

        auto win = CCDirector::get()->getWinSize();

        auto dim = CCLayerColor::create({0, 0, 0, 180});
        dim->setContentSize(win);
        this->addChild(dim);

        auto panel = CCScale9Sprite::create("GJ_square02.png");
        panel->setContentSize({310.f, 380.f});
        panel->setPosition(win / 2);
        panel->setColor({25, 25, 50});
        panel->setOpacity(240);
        this->addChild(panel);

        auto title = CCLabelBMFont::create("Stable Spam - CK", "bigFont.fnt");
        title->setScale(0.48f);
        title->setPosition({155.f, 340.f});
        panel->addChild(title);

        auto makeToggle = [&](const char* text, float y, bool* state, SEL_MenuHandler sel) {
            auto lab = CCLabelBMFont::create(text, "goldFont.fnt");
            lab->setScale(0.38f);
            lab->setAnchorPoint({0.f, 0.5f});
            lab->setPosition({35.f, y});
            panel->addChild(lab);

            auto on  = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
            auto off = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
            auto tog = CCMenuItemToggler::create(off, on, this, sel);
            tog->setPosition({260.f, y});
            tog->toggle(*state);
            return tog;
        };

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});

        menu->addChild(makeToggle("Mod activado",            290.f, &g_ckOn,           menu_selector(CKPanel::onToggleMain)));
        menu->addChild(makeToggle("Spam estable",           245.f, &g_enableSpam,     menu_selector(CKPanel::onToggleSpam)));
        menu->addChild(makeToggle("Straight Fly (nave)",    200.f, &g_enableStraight, menu_selector(CKPanel::onToggleStraight)));
        menu->addChild(makeToggle("Spam Wave fino",         155.f, &g_enableWave,     menu_selector(CKPanel::onToggleWave)));
        menu->addChild(makeToggle("Save Mode (anti-muerte)",110.f, &g_enableSave,     menu_selector(CKPanel::onToggleSave)));

        auto info = CCLabelBMFont::create(
            "Save Mode: te salva si fallas\npor poco (cooldown 1.5s)\nSolo para practicar",
            "chatFont.fnt"
        );
        info->setScale(0.48f);
        info->setAlignment(kCCTextAlignmentCenter);
        info->setPosition({155.f, 55.f});
        panel->addChild(info);

        auto close = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png"),
            this, menu_selector(CKPanel::onClose)
        );
        close->setPosition({280.f, 350.f});
        menu->addChild(close);

        panel->addChild(menu);
        return true;
    }

    void onToggleMain(CCObject*)     { g_ckOn = !g_ckOn; }
    void onToggleSpam(CCObject*)     { g_enableSpam = !g_enableSpam; }
    void onToggleStraight(CCObject*) { g_enableStraight = !g_enableStraight; }
    void onToggleWave(CCObject*)     { g_enableWave = !g_enableWave; }
    void onToggleSave(CCObject*)     { g_enableSave = !g_enableSave; }

    void onClose(CCObject*) {
        this->removeFromParentAndCleanup(true);
    }

    void keyBackClicked() override { this->onClose(nullptr); }
    bool ccTouchBegan(CCTouch*, CCEvent*) override { return true; }
};

// ===================== BOLITA ARRASTRABLE (versión simple y compatible) =====================
class DraggableCK : public CCLayer {
public:
    static DraggableCK* create(CCObject* target, SEL_MenuHandler callback) {
        auto ret = new DraggableCK();
        if (ret && ret->init(target, callback)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init(CCObject* target, SEL_MenuHandler callback) {
        if (!CCLayer::init()) return false;

        this->setContentSize({50.f, 50.f});
        this->setAnchorPoint({0.5f, 0.5f});
        this->ignoreAnchorPointForPosition(false);

        // Dibujo de la bolita
        auto dot = CCDrawNode::create();
        dot->setPosition({25.f, 25.f});
        dot->drawDot({0.f, 0.f}, 22.f, {0.f, 0.95f, 1.f, 1.f});
        dot->drawDot({0.f, 0.f}, 18.f, {0.05f, 0.08f, 0.32f, 0.95f});
        this->addChild(dot);

        auto lbl = CCLabelBMFont::create("CK", "bigFont.fnt");
        lbl->setScale(0.40f);
        lbl->setPosition({25.f, 25.f});
        lbl->setColor({0, 255, 255});
        this->addChild(lbl);

        m_target = target;
        m_callback = callback;
        m_dragging = false;

        this->setTouchEnabled(true);
        this->setTouchMode(kCCTouchesOneByOne);

        return true;
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        auto pos = this->convertToNodeSpace(touch->getLocation());
        auto size = this->getContentSize();

        if (pos.x >= 0 && pos.x <= size.width && pos.y >= 0 && pos.y <= size.height) {
            m_dragging = false;
            m_startPos = touch->getLocation();
            m_lastPos = m_startPos;
            return true;
        }
        return false;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
        auto current = touch->getLocation();
        auto diff = current - m_lastPos;

        if (!m_dragging) {
            auto total = current - m_startPos;
            if (ccpLength(total) > 8.f) {
                m_dragging = true;
            }
        }

        if (m_dragging) {
            this->setPosition(this->getPosition() + diff);
        }
        m_lastPos = current;
    }

    void ccTouchEnded(CCTouch* touch, CCEvent* event) override {
        if (!m_dragging && m_target && m_callback) {
            (m_target->*m_callback)(nullptr);
        }
        m_dragging = false;
    }

    void ccTouchCancelled(CCTouch* touch, CCEvent* event) override {
        m_dragging = false;
    }

private:
    bool m_dragging = false;
    CCPoint m_startPos;
    CCPoint m_lastPos;
    CCObject* m_target = nullptr;
    SEL_MenuHandler m_callback = nullptr;
};

// ===================== MENÚ PRINCIPAL =====================
class $modify(CKMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto win = CCDirector::get()->getWinSize();

        auto btn = DraggableCK::create(this, menu_selector(CKMenuLayer::onCK));
        btn->setPosition({win.width - 40.f, win.height - 100.f});
        this->addChild(btn, 100);

        return true;
    }

    void onCK(CCObject*) {
        auto scene = CCDirector::get()->getRunningScene();
        if (!scene || scene->getChildByTag(98765)) return;

        auto panel = CKPanel::create();
        panel->setTag(98765);
        scene->addChild(panel, 999);
    }
};

// ===================== LÓGICA DE SPAM =====================
class $modify(SpamGJBGL, GJBaseGameLayer) {
    struct Fields {
        std::deque<double> clicks;
        int mode = 0;
        bool held = false;
        bool p1 = true;
        double lastReal = 0.0;
        double acc = 0.0;
        double targetY = 0.0;
        int waveCount = 0;
    };

    PlayerObject* spamPlayer() {
        auto f = m_fields.self();
        return f->p1 ? m_player1 : m_player2;
    }

    void setHeld(bool h) {
        auto f = m_fields.self();
        if (f->held == h) return;
        f->held = h;
        GJBaseGameLayer::handleButton(h, 1, f->p1);
    }

    bool wantsUp(PlayerObject* pl, double look) {
        auto f = m_fields.self();
        double sgn = pl->m_isUpsideDown ? -1.0 : 1.0;
        double e = f->targetY - pl->getPositionY();
        return (e - pl->m_yVelocity * look) * sgn > 0.0;
    }

    void stopSpam() {
        auto f = m_fields.self();
        this->setHeld(false);
        f->mode = 0;
        f->acc = 0.0;
        f->waveCount = 0;
        f->clicks.clear();
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        auto f = m_fields.self();

        if (button != 1 || !PlayLayer::get()) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        if (!g_ckOn) {
            if (f->mode != 0) this->stopSpam();
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        double now = nowSec();

        if (f->mode == 2) {
            if (!down && isPlayer1 == f->p1) this->stopSpam();
            return;
        }

        if (f->mode == 1) {
            f->lastReal = now;
            return;
        }

        auto pl = isPlayer1 ? m_player1 : m_player2;

        if (down && pl && pl->m_isShip && g_enableStraight &&
            Mod::get()->getSettingValue<bool>("straightfly")) {
            f->mode = 2;
            f->p1 = isPlayer1;
            f->acc = 0.0;
            f->held = false;
            f->targetY = pl->getPositionY();
            return;
        }

        if (down && g_enableSpam) {
            auto window = Mod::get()->getSettingValue<double>("window");
            auto threshold = Mod::get()->getSettingValue<int64_t>("threshold");

            f->clicks.push_back(now);
            while (!f->clicks.empty() && now - f->clicks.front() > window) {
                f->clicks.pop_front();
            }

            if ((int64_t)f->clicks.size() >= threshold) {
                f->mode = 1;
                f->p1 = isPlayer1;
                f->lastReal = now;
                f->acc = 0.0;
                f->waveCount = 0;
                f->held = false;
                if (pl) f->targetY = pl->getPositionY();
            }
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto f = m_fields.self();

        if (f->mode != 0) {
            double now = nowSec();
            auto pl = this->spamPlayer();
            bool stop = !PlayLayer::get() || !g_ckOn;

            if (f->mode == 1) {
                auto timeout = Mod::get()->getSettingValue<double>("timeout");
                if (now - f->lastReal > timeout) stop = true;
            }
            if (f->mode == 2 && (!pl || !pl->m_isShip)) stop = true;

            if (stop) {
                this->stopSpam();
            } else if (f->mode == 2) {
                auto flylook = Mod::get()->getSettingValue<double>("flylook");
                this->setHeld(this->wantsUp(pl, flylook));
            } else if (pl && pl->m_isDart && g_enableWave) {
                auto n = (int)Mod::get()->getSettingValue<int64_t>("waveticks");
                if (n < 1) n = 1;
                f->waveCount++;
                if (f->waveCount >= n) {
                    f->waveCount = 0;
                    this->setHeld(!f->held);
                }
            } else if (g_enableSpam) {
                auto cps = (double)Mod::get()->getSettingValue<int64_t>("cps");
                auto stabilize = Mod::get()->getSettingValue<bool>("stabilize");
                auto look = Mod::get()->getSettingValue<double>("lookahead");
                double half = 0.5 / cps;
                bool control = stabilize && pl && pl->m_isShip;

                f->acc += dt;
                while (f->acc >= half) {
                    f->acc -= half;
                    if (control) {
                        this->setHeld(this->wantsUp(pl, look));
                    } else {
                        this->setHeld(!f->held);
                    }
                }
            }
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

// ===================== SAVE MODE =====================
class $modify(SavePlayer, PlayerObject) {
    void playerDestroyed(bool p0) {
        if (g_ckOn && g_enableSave) {
            double now = nowSec();
            if (now - g_lastSaveTime > SAVE_COOLDOWN) {
                g_lastSaveTime = now;
                return;
            }
        }
        PlayerObject::playerDestroyed(p0);
    }
};

// ===================== BOLITA DENTRO DEL NIVEL =====================
class $modify(CKUILayer, UILayer) {
    bool init(GJBaseGameLayer* layer) {
        if (!UILayer::init(layer)) return false;
        if (!typeinfo_cast<PlayLayer*>(layer)) return true;

        auto win = CCDirector::get()->getWinSize();

        auto btn = DraggableCK::create(this, menu_selector(CKUILayer::onCK));
        btn->setPosition({win.width - 40.f, win.height - 115.f});
        this->addChild(btn, 200);

        return true;
    }

    void onCK(CCObject*) {
        auto scene = CCDirector::get()->getRunningScene();
        if (!scene || scene->getChildByTag(98765)) return;

        auto panel = CKPanel::create();
        panel->setTag(98765);
        scene->addChild(panel, 999);
    }
};
