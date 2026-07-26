#pragma once
// ===========================================================================
// YLifetimeModule
//
// 「粒子1個あたりに1組だけ存在するパラメータ」を担当するParamモジュール。
// ForceField/Noiseのような可変長インスタンス配列(YGpuFieldArrayModule)とは違い、
// 専用GPUバッファは持たず、YGpuEmitter::ParticleParameters という単一CBVの
// 担当フィールドへ書き込むだけ。
//
// 責務: 寿命(lifeTime/lifeTimeVariance)の編集とCBVへの書き込みのみ。
// ===========================================================================
#include <GPUParticle/YGpuEmitter.h>
#include <GPUParticle/GpuParticleParams.h>

namespace YLifetimeModule {

	// CPU側 ParticleParams から GPU CBV(ParticleParameters) へ担当フィールドを書き込む
	void WriteTo(YGpuEmitter::ParticleParameters& dst, const ParticleParams& src);

#ifdef USE_IMGUI
	// ImGui編集ブロックを描画する。変更があれば true を返す。
	bool DrawImGui(ParticleParams& params);
#endif

} // namespace YLifetimeModule
