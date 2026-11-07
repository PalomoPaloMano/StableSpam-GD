 #include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <chrono>
#include <deque>

using namespace geode::prelude;

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

	void stopSpam() {
		auto f = m_fields.self();
		this->setHeld(false);
		f->mode = 0;
		f->acc = 0.0;
		f->clicks.clear();
	}

	void handleButton(bool down, int button, bool isPlayer1) {
		auto f = m_fields.self();

		if (button != 1 || !PlayLayer::get()) {
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
			bool stop = !PlayLayer::get();

			if (f->mode == 1) {
				auto timeout = Mod::get()->getSettingValue<double>("timeout");
				if (now - f->lastReal > timeout) stop = true;
			}
			if (f->mode == 2 && (!pl || !pl->m_isShip)) stop = true;

			if (stop) {
				this->stopSpam();
			} else {
				auto cps = (double)Mod::get()->getSettingValue<int64_t>("cps");
				auto stabilize = Mod::get()->getSettingValue<bool>("stabilize");
				auto look = Mod::get()->getSettingValue<double>("lookahead");
				double half = 0.5 / cps;
				bool control = (f->mode == 2) || (stabilize && pl && pl->m_isShip);

				f->acc += dt;
				while (f->acc >= half) {
					f->acc -= half;
					if (control && pl) {
						double sgn = pl->m_isUpsideDown ? -1.0 : 1.0;
						double err = (f->targetY - pl->getPositionY()) * sgn
							- pl->m_yVelocity * look;
						this->setHeld(err > 0.0);
					} else {
						this->setHeld(!f->held);
					}
				}
			}
		}

		GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
	}
};
