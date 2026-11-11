// PerfBoost v1.1 - mod tối ưu hiệu năng cho Geometry Dash (Geode)
// Đặt file này vào src/main.cpp

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/ShaderLayer.hpp>
#include <Geode/modify/CCParticleSystem.hpp>

using namespace geode::prelude;

// ---------------------------------------------------------------------
// Cấu hình đệm (cache) để không phải đọc setting mỗi frame
//   level: 0 = Custom, 1 = Balanced, 2 = Max Performance
// ---------------------------------------------------------------------
namespace cfg {
	inline int level = 0;

	inline bool rawShaders = true;
	inline bool rawGlow = true;
	inline int64_t rawParticles = 30;
	inline bool rawDeath = false;
	inline bool rawBackground = false;
	inline bool rawGround = false;
	inline bool rawProgress = false;
	inline bool rawTrail = false;
	inline bool rawWave = false;
	inline int64_t rawFps = 0;

	inline bool noShaders() { return level >= 1 || rawShaders; }
	inline bool noGlow() { return level >= 1 || rawGlow; }
	inline bool noDeathEffect() { return level >= 2 || rawDeath; }
	inline bool hideBackground() { return level >= 2 || rawBackground; }
	inline bool hideGround() { return level >= 2 || rawGround; }
	inline bool hideProgress() { return rawProgress; }
	inline bool hideTrail() { return level >= 1 || rawTrail; }
	inline bool hideWaveTrail() { return rawWave; }

	inline int64_t particlePercent() {
		if (level >= 2) return 0;
		if (level == 1) return std::min<int64_t>(rawParticles, 30);
		return rawParticles;
	}

	inline int64_t maxFps() {
		if (rawFps > 0) return rawFps;
		if (level >= 2) return 30;
		return 0;
	}
}

static int levelFromString(std::string const& s) {
	if (s == "Max Performance") return 2;
	if (s == "Balanced") return 1;
	return 0;
}

static void applyFps() {
	auto fps = cfg::maxFps();
	if (fps <= 0) return;
	auto app = CCApplication::sharedApplication();
	if (!app) return;
	app->setAnimationInterval(1.0 / static_cast<double>(fps));
}

template <class T>
static void bindSetting(char const* key, T& target) {
	target = Mod::get()->getSettingValue<T>(key);
	listenForSettingChanges<T>(key, [&target](T value) {
		target = value;
		applyFps();
	});
}

$on_mod(Loaded) {
	cfg::level = levelFromString(Mod::get()->getSettingValue<std::string>("preset"));
	listenForSettingChanges<std::string>("preset", [](std::string value) {
		cfg::level = levelFromString(value);
		applyFps();
	});

	bindSetting<int64_t>("max-fps", cfg::rawFps);
	bindSetting<bool>("disable-shaders", cfg::rawShaders);
	bindSetting<bool>("disable-glow", cfg::rawGlow);
	bindSetting<int64_t>("particle-percent", cfg::rawParticles);
	bindSetting<bool>("disable-death-effect", cfg::rawDeath);
	bindSetting<bool>("hide-background", cfg::rawBackground);
	bindSetting<bool>("hide-ground", cfg::rawGround);
	bindSetting<bool>("hide-progress", cfg::rawProgress);
	bindSetting<bool>("hide-trail", cfg::rawTrail);
	bindSetting<bool>("hide-wave-trail", cfg::rawWave);
}

// ---------------------------------------------------------------------
// 1) Particle: giảm lượng phát (hoặc tắt hẳn) cho particle trong level
//    Mỗi particle system chỉ được chỉnh 1 lần (đánh dấu bằng user object)
// ---------------------------------------------------------------------
class $modify(PBParticle, CCParticleSystem) {
	void update(float dt) {
		auto percent = cfg::particlePercent();
		if (percent < 100 && PlayLayer::get()) {
			auto key = "optimized"_spr;
			if (!this->getUserObject(key)) {
				this->setUserObject(key, CCInteger::create(1));
				if (percent <= 0) {
					this->setEmissionRate(0.f);
				}
				else {
					this->setEmissionRate(
						this->getEmissionRate() * static_cast<float>(percent) / 100.f
					);
				}
			}
			if (percent <= 0) {
				this->setVisible(false);
			}
		}
		CCParticleSystem::update(dt);
	}
};

// ---------------------------------------------------------------------
// 2) Shader: bỏ qua ShaderLayer, vẽ thẳng con của nó (không render-to-texture)
// ---------------------------------------------------------------------
class $modify(PBShaderLayer, ShaderLayer) {
	void visit() {
		if (cfg::noShaders()) {
			CCNode::visit();
			return;
		}
		ShaderLayer::visit();
	}
};

// ---------------------------------------------------------------------
// 3) Glow của object: không tạo glow trong level (editor giữ nguyên)
// ---------------------------------------------------------------------
class $modify(PBGameObject, GameObject) {
	void addGlow(gd::string frame) {
		if (cfg::noGlow() && PlayLayer::get()) return;
		GameObject::addGlow(frame);
	}
};

// ---------------------------------------------------------------------
// 4) Hiệu ứng chết
// ---------------------------------------------------------------------
class $modify(PBPlayerObject, PlayerObject) {
	void playDeathEffect() {
		if (cfg::noDeathEffect()) return;
		PlayerObject::playDeathEffect();
	}
};

// ---------------------------------------------------------------------
// 5) PlayLayer: ẩn nền/mặt đất/HUD khi vào level, ẩn vệt đuôi mỗi frame,
//    áp dụng giới hạn FPS
// ---------------------------------------------------------------------
class $modify(PBPlayLayer, PlayLayer) {
	void setupHasCompleted() {
		PlayLayer::setupHasCompleted();

		if (cfg::hideBackground() && m_background) {
			m_background->setVisible(false);
		}
		if (cfg::hideGround()) {
			if (m_groundLayer) m_groundLayer->setVisible(false);
			if (m_groundLayer2) m_groundLayer2->setVisible(false);
		}
		if (cfg::hideProgress()) {
			if (m_progressBar) m_progressBar->setVisible(false);
			if (m_percentageLabel) m_percentageLabel->setVisible(false);
		}

		applyFps();
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);

		bool trail = cfg::hideTrail();
		bool wave = cfg::hideWaveTrail();
		if (!trail && !wave) return;

		for (auto* player : {m_player1, m_player2}) {
			if (!player) continue;
			if (trail && player->m_regularTrail) {
				player->m_regularTrail->setVisible(false);
			}
			if (wave && player->m_waveTrail) {
				player->m_waveTrail->setVisible(false);
			}
		}
	}
};
