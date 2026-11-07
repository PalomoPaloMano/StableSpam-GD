#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/UILayer.hpp>
#include <chrono>
#include <deque>

using namespace geode::prelude;

static bool g_ckOn = true;
static ccColor4B g_panelBgColor = {25, 25, 45, 235};

static double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// ===================== PANEL =====================
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

        // Fondo oscuro
        auto dim = CCLayerColor::create({0, 0, 0, 170});
        dim->setContentSize(win);
        this->addChild(dim);

        // Panel
        auto panel = CCScale9Sprite::create("GJ_square02.png");
        panel->setContentSize({300.f, 340.f});
        panel->setPosition(win / 2);
        panel->setColor({g_panelBgColor.r, g_panelBgColor.g, g_panelBgColor.b});
        panel->setOpacity(g_panelBgColor.a);
        this->addChild(panel);
        m_panel = panel;

        // Título
        auto title = CCLabelBMFont::create("Stable Spam", "bigFont.fnt");
        title->setScale(0.5f);
        title->setPosition({150.f, 300.f});
        panel->addChild(title);

        // --- Apartado 1: On/Off ---
        auto lab1 = CCLabelBMFont::create("1. Estado del mod", "goldFont.fnt");
        lab1->setScale(0.38f);
        lab1->setPosition({150.f, 255.f});
        panel->addChild(lab1);

        auto onSpr = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
        auto offSpr = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
        auto toggle = CCMenuItemToggler::create(offSpr, onSpr, this, menu_selector(CKPanel::onToggle));
        toggle->setPosition({150.f, 215.f});
        toggle->toggle(g_ckOn);

        // --- Apartado 2: Colores ---
        auto lab2 = CCLabelBMFont::create("2. Color del panel", "goldFont.fnt");
        lab2->setScale(0.38f);
        lab2->setPosition({150.f, 170.f});
        panel->addChild(lab2);

        auto colorMenu = CCMenu::create();
        colorMenu->setPosition({0.f, 0.f});

        std::vector<ccColor3B> cols = {
            {25, 25, 45}, {50, 20, 70}, {20, 45, 30},
            {70, 20, 20}, {30, 30, 55}, {15, 15, 15}
        };

        float x = 55.f;
        for (size_t i = 0; i < cols.size(); ++i) {
            auto spr = CCLayerColor::create(
                {cols[i].r, cols[i].g, cols[i].b, 255}, 34.f, 34.f
            );
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CKPanel::onColor));
            btn->setTag(static_cast<int>(i));
            btn->setPosition({x + i * 40.f, 125.f});
            colorMenu->addChild(btn);
        }
        panel->addChild(colorMenu);

        // --- Apartado 3 ---
        auto lab3 = CCLabelBMFont::create("3. Info", "goldFont.fnt");
        lab3->setScale(0.38f);
        lab3->setPosition({150.f, 80.f});
        panel->addChild(lab3);

        auto info = CCLabelBMFont::create(
            "Spam estable + Straight Fly\nWave fina (ticks = 1)\nSolo practica",
            "chatFont.fnt"
        );
        info->setScale(0.5f);
        info->setAlignment(kCCTextAlignmentCenter);
        info->setPosition({150.f, 45.f});
        panel->addChild(info);

        // Cerrar
        auto close = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png"),
            this, menu_selector(CKPanel::onClose)
        );
        close->setPosition({270.f, 310.f});

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        menu->addChild(toggle);
        menu->addChild(close);
        panel->addChild(menu);

        return true;
    }

    void onToggle(CCObject*) {
        g_ckOn = !g_ckOn;
    }

    void onColor(CCObject* sender) {
        auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
        int tag = btn->getTag();

        std::vector<ccColor4B> cols = {
            {25, 25, 45, 235}, {50, 20, 70, 235}, {20, 45, 30, 235},
            {70, 20, 20, 235}, {30, 30, 55, 235}, {15, 15, 15, 235}
        };

        if (tag >= 0 && tag < (int)cols.size()) {
            g_panelBgColor = cols[tag];
            if (m_panel) {
                m_panel->setColor({g_panelBgColor.r, g_panelBgColor.g, g_panelBgColor.b});
                m_panel->setOpacity(g_panelBgColor.a);
            }
        }
    }

    void onClose(CCObject*) {
        this->removeFromParentAndCleanup(true);
    }

    void keyBackClicked() override {
        this->onClose(nullptr);
    }

    bool ccTouchBegan(CCTouch*, CCEvent*) override {
        return true;
    }

private:
    CCScale9Sprite* m_panel = nullptr;
};

// ===================== SPAM LOGIC =====================
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

        if (down && pl && pl->m_isShip &&
            Mod::get()->getSettingValue<bool>("straightfly")) {
            f->mode = 2;
            f->p1 = isPlayer1;
            f->acc = 0.0;
            f->held = false;
            f->targetY = pl->getPositionY();
            return;
        }

        if (down) {
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
            } else if (pl && pl->m_isDart) {
                auto n = (int)Mod::get()->getSettingValue<int64_t>("waveticks");
                if (n < 1) n = 1;
                f->waveCount++;
                if (f->waveCount >= n) {
                    f->waveCount = 0;
                    this->setHeld(!f->held);
                }
            } else {
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

// ===================== BOLITA CK =====================
class $modify(CKUILayer, UILayer) {
    struct Fields {
        CCDrawNode* dot = nullptr;
        CCLabelBMFont* lbl = nullptr;
    };

    void refreshCK() {
        auto f = m_fields.self();
        if (!f->dot) return;

        f->dot->clear();
        ccColor4F ring = g_ckOn ? ccColor4F{0.f, 0.95f, 1.f, 1.f} : ccColor4F{0.4f, 0.4f, 0.4f, 1.f};
        ccColor4F fill = g_ckOn ? ccColor4F{0.05f, 0.1f, 0.35f, 0.95f} : ccColor4F{0.12f, 0.12f, 0.12f, 0.95f};

        f->dot->drawDot({0.f, 0.f}, 22.f, ring);
        f->dot->drawDot({0.f, 0.f}, 18.5f, fill);

        if (f->lbl) {
            f->lbl->setColor(g_ckOn ? ccc3(0, 255, 255) : ccc3(140, 140, 140));
        }
    }

    void onCK(CCObject*) {
        auto scene = CCDirector::get()->getRunningScene();
        if (!scene) return;

        // Evitar abrir varios paneles
        if (scene->getChildByTag(98765)) return;

        auto panel = CKPanel::create();
        panel->setTag(98765);
        scene->addChild(panel, 999);
    }

    bool init(GJBaseGameLayer* layer) {
        if (!UILayer::init(layer)) return false;
        if (!typeinfo_cast<PlayLayer*>(layer)) return true;

        auto f = m_fields.self();
        auto win = CCDirector::get()->getWinSize();

        auto node = CCNode::create();
        node->setContentSize({48.f, 48.f});

        f->dot = CCDrawNode::create();
        f->dot->setPosition({24.f, 24.f});
        node->addChild(f->dot);

        f->lbl = CCLabelBMFont::create("CK", "bigFont.fnt");
        f->lbl->setScale(0.4f);
        f->lbl->setPosition({24.f, 24.f});
        node->addChild(f->lbl);

        auto btn = CCMenuItemSpriteExtra::create(
            node, this, menu_selector(CKUILayer::onCK)
        );
        btn->setPosition({win.width - 36.f, win.height - 110.f});

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        menu->addChild(btn);
        this->addChild(menu, 200);

        this->refreshCK();
        return true;
    }
};
