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
		bool active = false;
		bool held = false;
		bool p1 = true;
		double lastReal = 0.0;
		double acc = 0.0;
	};

	void stopSpam() {
		auto f = m_fields.self();
		if (f->held) {
			GJBaseGameLayer::handleButton(false, 1, f->p1);
			f->held = false;
		}
		f->active = false;
		f->acc = 0.0;
		f->clicks.clear();
	}

	void handleButton(bool down, int button, bool isPlayer1) {
		auto f = m_fields.self();

		if (button != 1 || !PlayLayer::get()) {
			return GJBaseGameLayer::handleButton(down, button, isPlayer1);
		}

		double now = nowSec();

		if (f->active) {
			f->lastReal = now;
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
				f->active = true;
				f->p1 = isPlayer1;
				f->lastReal = now;
				f->acc = 0.0;
				f->held = false;
			}
		}

		GJBaseGameLayer::handleButton(down, button, isPlayer1);
	}

	void processCommands(float dt) {
		auto f = m_fields.self();

		if (f->active) {
			double now = nowSec();
			auto timeout = Mod::get()->getSettingValue<double>("timeout");

			if (now - f->lastReal > timeout || !PlayLayer::get()) {
				this->stopSpam();
			} else {
				auto cps = (double)Mod::get()->getSettingValue<int64_t>("cps");
				double half = 0.5 / cps;

				f->acc += dt;
				while (f->acc >= half) {
					f->acc -= half;
					f->held = !f->held;
					GJBaseGameLayer::handleButton(f->held, 1, f->p1);
				}
			}
		}

		GJBaseGameLayer::processCommands(dt);
	}
};
