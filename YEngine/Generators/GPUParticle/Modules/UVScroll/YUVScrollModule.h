#pragma once
// ===========================================================================
// YUVScrollModule
//
// 拡張Paramモジュール（任意ON/OFF、エディタで追加・削除できる演出）。
// テクスチャUVを一定速度でスクロールさせる。炎・溶岩・エネルギー系エフェクトの定番。
// Fieldモジュールと違い専用GPUバッファは持たず、YGpuParticle::ParticleExtParameters
// という共有CBV(b1)の担当フィールドへ書き込むだけ。
// ===========================================================================
#include <GPUParticle/YGpuParticle.h>
#include <Vector2.h>

struct UVScrollParams {
	bool    isEnable = false;         // エディタの「追加」でtrueになる
	Vector2 scrollSpeed = { 0.1f, 0.0f }; // UV/秒
};

namespace YUVScrollModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const UVScrollParams& src);

#ifdef USE_IMGUI
	bool DrawImGui(UVScrollParams& params);
#endif

} // namespace YUVScrollModule
