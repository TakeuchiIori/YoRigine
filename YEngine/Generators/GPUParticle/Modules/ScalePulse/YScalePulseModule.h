#pragma once
// ===========================================================================
// YScalePulseModule
//
// 拡張Paramモジュール（任意ON/OFF）。粒子の経過時間(hot.currentTime)を位相にした
// sin波でスケールを脈動させる。発光オーブ・鼓動する魔法陣などの定番演出。
// 基本スケール(YScaleOverLifeModule)の"上に乗る"乗算演出という位置づけ。
// ===========================================================================
#include <GPUParticle/YGpuParticle.h>

struct ScalePulseParams {
	bool  isEnable = false;
	float amplitude = 0.2f;  // 脈動の振幅（±何%スケールを揺らすか）
	float frequency = 1.0f;  // 脈動の周波数 (Hz)
};

namespace YScalePulseModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const ScalePulseParams& src);

#ifdef USE_IMGUI
	bool DrawImGui(ScalePulseParams& params);
#endif

} // namespace YScalePulseModule
