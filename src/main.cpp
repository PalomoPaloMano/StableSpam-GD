#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/UILayer.hpp>
#include <chrono>
#include <deque>

using namespace geode::prelude;

static bool g_ckOn = true;

static double nowSec() {
	using namespace std::chrono;
	return duration<double>(steady_clock::now().time_since_epoch()).count();
}

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

		// Mod apagado con la bolita CK
		if (!g_ckOn) {
			if (f->mode != 0) this->stopSpam();
			return GJBaseGameLayer::handleButton(down, button, isPlayer1);
		}

		double now = nowSec();

		// Straight fly activo: soltar la pantalla lo termina
		if (f->mode == 2) {
			if (!down && isPlayer1 == f->p1) this->stopSpam();
			return;
		}

		// Spam activo: tus toques solo lo mantienen vivo
		if (f->mode == 1) {
			f->lastReal = now;
			return;
		}

		auto pl = isPlayer1 ? m_player1 : m_player2;

		// Iniciar straight fly (solo nave)
		if (down && pl && pl->m_isShip &&
			Mod::get()->getSettingValue<bool>("straightfly")) {
			f->mode = 2;
			f->p1 = isPlayer1;
			f->acc = 0.0;
			f->held = false;
			f->targetY = pl->getPositionY();
			return;
		}

		// Detección de spam
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
				// Straight fly: decide en cada tick para ir recto
				auto flylook = Mod::get()->getSettingValue<double>("flylook");
				this->setHeld(this->wantsUp(pl, flylook));
			} else if (pl && pl->m_isDart) {
				// Wave: zigzag lo más fino posible, cambia cada N ticks
				auto n = (int)Mod::get()->getSettingValue<int64_t>("waveticks");
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

class $modify(CKUILayer, UILayer) {
	struct Fields {
		CCDrawNode* dot = nullptr;
	};

	void refreshCK() {
		auto f = m_fields.self();
		if (!f->dot) return;
		f->dot->clear();
		ccColor4F ring = g_ckOn ? ccColor4F{0.f, 0.9f, 1.f, 1.f}
		                        : ccColor4F{0.5f, 0.5f, 0.5f, 1.f};
		ccColor4F fill = g_ckOn ? ccColor4F{0.1f, 0.1f, 0.4f, 0.95f}
		                        : ccColor4F{0.15f, 0.15f, 0.15f, 0.95f};
		f->dot->drawDot({0.f, 0.f}, 20.f, ring);
		f->dot->drawDot({0.f, 0.f}, 17.f, fill);
	}

	void onCK(CCObject*) {
		g_ckOn = !g_ckOn;
		this->refreshCK();
	}

	bool init(GJBaseGameLayer* layer) {
		if (!UILayer::init(layer)) return false;
		if (!typeinfo_cast<PlayLayer*>(layer)) return true;

		auto f = m_fields.self();
		auto win = CCDirector::get()->getWinSize();

		auto node = CCNode::create();
		node->setContentSize({40.f, 40.f});

		f->dot = CCDrawNode::create();
		f->dot->setPosition({20.f, 20.f});
		node->addChild(f->dot);

		auto lbl = CCLabelBMFont::create("CK", "bigFont.fnt");
		lbl->setScale(0.45f);
		lbl->setPosition({20.f, 20.f});
		node->addChild(lbl);

		auto btn = CCMenuItemSpriteExtra::create(
			node, this, menu_selector(CKUILayer::onCK)
		);
		btn->setPosition({win.width - 30.f, win.height - 130.f});

		auto menu = CCMenu::create();
		menu->setPosition({0.f, 0.f});
		menu->addChild(btn);
		this->addChild(menu, 100);

		this->refreshCK();
		return true;
	}
};
