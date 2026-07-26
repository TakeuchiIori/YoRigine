#pragma once
// ===========================================================================
// YColorFlickerModule
//
// 拡張Paramモジュール（任意ON/OFF）。粒子ごとに位相をずらしたsin波で明るさを
// 明滅させる。炎・電撃・魔法のちらつき表現の定番。
// ===========================================================================
#include <GPUParticle/YGpuParticle.h>

struct ColorFlickerParams {
	bool  isEnable = false;
	float speed = 8.0f;      // 明滅の速さ (Hz)
	float intensity = 0.3f;  // 明滅の強さ（±何%明るさを揺らすか）
};

namespace YColorFlickerModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const ColorFlickerParams& src);

#ifdef USE_IMGUI
	bool DrawImGui(ColorFlickerParams& params);
#endif

} // namespace YColorFlickerModule
