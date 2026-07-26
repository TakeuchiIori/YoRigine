#pragma once
// ===========================================================================
// YStretchByVelocityModule
//
// 拡張Paramモジュール（任意ON/OFF）。粒子を速度方向へ引き伸ばす。
// 火花・スピード線・雨などの定番で、伸びるだけで一気に「速い」印象になる。
//
// GPU側は VS が ParticleExtParams(b1) を読み、
//   ・速度をスクリーン平面へ投影して粒子の向きを速度方向へ揃える
//   ・速さに比例して伸ばす方向のスケールを増やす
// を行う。有効時は hot.rotate による回転を上書きする（火花は速度を向くべきで、
// ランダム回転と併用する意味がないため）。
// ===========================================================================
#include <GPUParticle/YGpuParticle.h>

struct StretchByVelocityParams {
	bool  isEnable = false;
	float scale = 0.1f;      // 速さ1あたり何倍伸ばすか
	float maxStretch = 5.0f; // 伸びの上限倍率（高速時に伸びすぎないようクランプ）
};

namespace YStretchByVelocityModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const StretchByVelocityParams& src);

#ifdef USE_IMGUI
	bool DrawImGui(StretchByVelocityParams& params);
#endif

} // namespace YStretchByVelocityModule
