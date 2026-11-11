// PerfBoost - mod tối ưu hiệu năng cho Geometry Dash (Geode)
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
// ---------------------------------------------------------------------
namespace cfg {
	inline bool noShaders = true;
	inline bool noGlow = true;
	inline int64_t particlePercent = 30;
	inline bool noDeathEffect = false;
	inline bool hideBackground = false;
}

template <class T>
static void bindSetting(char const* key, T& target) {
	target = Mod::get()->getSettingValue<T>(key);
	listenForSettingChanges<T>(key, [&target](T value) {
		target = value;
	});
}

$on_mod(Loaded) {
	bindSetting<bool>("disable-shaders", cfg::noShaders);
	bindSetting<bool>("disable-glow", cfg::noGlow);
	bindSetting<int64_t>("particle-percent", cfg::particlePercent);
	bindSetting<bool>("disable-death-effect", cfg::noDeathEffect);
	bindSetting<bool>("hide-background", cfg::hideBackground);
}

// ---------------------------------------------------------------------
// 1) Particle: giảm lượng phát (hoặc tắt hẳn) cho particle trong level
//    Mỗi particle system chỉ được chỉnh 1 lần (đánh dấu bằng user object)
// ---------------------------------------------------------------------
class $modify(PBParticle, CCParticleSystem) {
	void update(float dt) {
		if (cfg::particlePercent < 100 && PlayLayer::get()) {
			auto key = "optimized"_spr;
			if (!this->getUserObject(key)) {
				this->setUserObject(key, CCInteger::create(1));
				if (cfg::particlePercent <= 0) {
					this->setEmissionRate(0.f);
				}
				else {
					this->setEmissionRate(
						this->getEmissionRate() * static_cast<float>(cfg::particlePercent) / 100.f
					);
				}
			}
			if (cfg::particlePercent <= 0) {
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
		if (cfg::noShaders) {
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
		if (cfg::noGlow && PlayLayer::get()) return;
		GameObject::addGlow(frame);
	}
};

// ---------------------------------------------------------------------
// 4) Hiệu ứng chết
// ---------------------------------------------------------------------
class $modify(PBPlayerObject, PlayerObject) {
	void playDeathEffect() {
		if (cfg::noDeathEffect) return;
		PlayerObject::playDeathEffect();
	}
};

// ---------------------------------------------------------------------
// 5) Ẩn nền khi vào level
// ---------------------------------------------------------------------
class $modify(PBPlayLayer, PlayLayer) {
	void setupHasCompleted() {
		PlayLayer::setupHasCompleted();
		if (cfg::hideBackground && m_background) {
			m_background->setVisible(false);
		}
	}
};
