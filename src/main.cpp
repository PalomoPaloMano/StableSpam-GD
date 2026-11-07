#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/UILayer.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <chrono>
#include <deque>

using namespace geode::prelude;

static bool g_ckOn = true;
static ccColor4B g_panelBgColor = {20, 20, 40, 230}; // color por defecto del panel

static double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// ===================== PANEL DE CONFIGURACIÓN =====================
class CKPanel : public CCLayer {
public:
    static CKPanel* create() {
        auto p = new CKPanel();
        if (p && p->init()) {
            p->autorelease();
            return p;
        }
        CC_SAFE_DELETE(p);
        return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;

        this->setTouchEnabled(true);
        this->setKeypadEnabled(true);

        auto win = CCDirector::get()->getWinSize();

        // Fondo semi-transparente que bloquea toques
        auto bg = CCLayerColor::create({0, 0, 0, 160});
        bg->setContentSize(win);
        this->addChild(bg);

        // Panel principal
        auto panel = CCScale9Sprite::create("GJ_square02.png");
        panel->setContentSize({320.f, 380.f});
        panel->setPosition(win / 2);
        panel->setColor({g_panelBgColor.r, g_panelBgColor.g, g_panelBgColor.b});
        panel->setOpacity(g_panelBgColor.a);
        this->addChild(panel);
        m_panel = panel;

        // Título
        auto title = CCLabelBMFont::create("Stable Spam - CK", "bigFont.fnt");
        title->setScale(0.55f);
        title->setPosition({160.f, 340.f});
        panel->addChild(title);

        // ===== APARTADO 1: Activar / Desactivar =====
        auto lab1 = CCLabelBMFont::create("1. Estado del mod", "goldFont.fnt");
        lab1->setScale(0.4f);
        lab1->setPosition({160.f, 290.f});
        panel->addChild(lab1);

        auto btnOnOff = CCMenuItemToggler::create(
            CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png"),
            CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png"),
            this, menu_selector(CKPanel::onToggleMod)
        );
        btnOnOff->setPosition({160.f, 250.f});
        btnOnOff->toggle(g_ckOn);

        // ===== APARTADO 2: Color de fondo =====
        auto lab2 = CCLabelBMFont::create("2. Color del panel", "goldFont.fnt");
        lab2->setScale(0.4f);
        lab2->setPosition({160.f, 200.f});
        panel->addChild(lab2);

        // Botones de colores rápidos
        auto colors = std::vector<ccColor3B>{
            {20, 20, 40}, {40, 20, 60}, {20, 40, 30},
            {60, 20, 20}, {30, 30, 50}, {10, 10, 10}
        };

        auto colorMenu = CCMenu::create();
        colorMenu->setPosition({0, 0});

        float startX = 60.f;
        for (size_t i = 0; i < colors.size(); i++) {
            auto spr = CCLayerColor::create({colors[i].r, colors[i].g, colors[i].b, 255}, 36, 36);
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CKPanel::onColor));
            btn->setTag(i);
            btn->setPosition({startX + i * 42.f, 155.f});
            colorMenu->addChild(btn);
        }
        panel->addChild(colorMenu);

        // ===== APARTADO 3: Info + Cerrar =====
        auto lab3 = CCLabelBMFont::create("3. Info", "goldFont.fnt");
        lab3->setScale(0.4f);
        lab3->setPosition({160.f, 100.f});
        panel->addChild(lab3);

        auto info = CCLabelBMFont::create(
            "Spam estable + Straight Fly\nWave: linea fina (ticks=1)\nPractica solamente",
            "chatFont.fnt"
        );
        info->setScale(0.55f);
        info->setAlignment(kCCTextAlignmentCenter);
        info->setPosition({160.f, 60.f});
        panel->addChild(info);

        // Botón cerrar
        auto closeBtn = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png"),
            this, menu_selector(CKPanel::onClose)
        );
        closeBtn->setPosition({290.f, 350.f});

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        menu->addChild(btnOnOff);
        menu->addChild(closeBtn);
        panel->addChild(menu);

        return true;
    }

    void onToggleMod(CCObject* sender) {
        g_ckOn = !g_ckOn;
        // Notificar al UILayer para que refresque la bolita
        if (auto layer = UILayer::get()) {
            if (auto ck = typeinfo_cast<CKUILayer*>(layer)) {
                ck->refreshCK();
            }
        }
    }

    void onColor(CCObject* sender) {
        auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
        int tag = btn->getTag();

        std::vector<ccColor4B> colors = {
            {20, 20, 40, 230}, {40, 20, 60, 230}, {20, 40, 30, 230},
            {60, 20, 20, 230}, {30, 30, 50, 230}, {10, 10, 10, 230}
        };

        if (tag >= 0 && tag < (int)colors.size()) {
            g_panelBgColor = colors[tag];
            if (m_panel) {
                m_panel->setColor({g_panelBgColor.r, g_panelBgColor.g, g_panelBgColor.b});
                m_panel->setOpacity(g_panelBgColor.a);
            }
        }
    }

    void onClose(CCObject*) {
        this->removeFromParent();
    }

    void keyBackClicked() override {
        this->onClose(nullptr);
    }

    // Bloquear toques al fondo
    bool ccTouchBegan(CCTouch*, CCEvent*) override { return true; }

private:
    CCScale9Sprite* m_panel = nullptr;
};

// ===================== LÓGICA DE SPAM =====================
class $modify(SpamGJBGL, GJBaseGameLayer) {
    struct Fields {
        std::deque<double> clicks;
        int mode = 0; // 0 = apagado, 1 = spam, 2 = straight fly
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
        auto f = m
