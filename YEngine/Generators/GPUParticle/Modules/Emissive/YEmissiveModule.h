#pragma once
// ===========================================================================
// YEmissiveModule
//
// 拡張Paramモジュール（任意ON/OFF）。粒子色のRGBに輝度倍率を掛け、1.0を超える
// HDR値へ持ち上げる。
//
// なぜ必要か: シーンのレンダーターゲットはHDR(kSceneColorFormat)で Bloom も
// 通っているのに、色の指定は ImGui::ColorEdit4 の 0〜1 に制限されているため、
// 「光っている粒子」を作れなかった。この倍率を掛けることで初めて Bloom が乗る。
// 発光弾・魔法・火花・レーザーなど発光系エフェクトの前提になる。
//
// GPU側は VS が ParticleExtParams(b1) を読み、color.rgb に乗算する
// （アルファは変更しない＝透過の形は保ったまま明るさだけ上げる）。
// ===========================================================================
#include <GPUParticle/YGpuParticle.h>

struct EmissiveParams {
	bool  isEnable = false;
	float intensity = 3.0f; // RGB倍率。1=変化なし、1超でBloomが乗り始める
};

namespace YEmissiveModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const EmissiveParams& src);

#ifdef USE_IMGUI
	bool DrawImGui(EmissiveParams& params);
#endif

} // namespace YEmissiveModule
