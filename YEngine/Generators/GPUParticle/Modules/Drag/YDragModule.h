#pragma once
// ===========================================================================
// YDragModule
//
// 拡張Paramモジュール（任意ON/OFF）。速度に空気抵抗をかけて減速させる。
// 爆散した破片が失速して落ちる、煙が広がりながら止まる、といった
// 「勢いが死んでいく」表現の土台。ほぼ全ての爆発系エフェクトで使う。
//
// GPU側は Update CS が ParticleExtParams(b2) の担当フィールドを読み、
//   velocity *= saturate(1 - drag * dt)
// で毎フレーム減衰させる（指数減衰の1次近似）。
// ===========================================================================
#include <GPUParticle/YGpuParticle.h>

struct DragParams {
	bool  isEnable = false;
	float coefficient = 1.0f; // 抵抗係数。大きいほど早く止まる（1/秒）
};

namespace YDragModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const DragParams& src);

#ifdef USE_IMGUI
	bool DrawImGui(DragParams& params);
#endif

} // namespace YDragModule
