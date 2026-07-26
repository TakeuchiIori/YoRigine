#pragma once
// ===========================================================================
// YScaleOverLifeModule
//
// 開始/終了スケール（Age基準の補間はVS側で導出、ここはCBVへの値供給のみ担当）。
// ===========================================================================
#include <GPUParticle/YGpuEmitter.h>
#include <GPUParticle/GpuParticleParams.h>

namespace YScaleOverLifeModule {

	void WriteTo(YGpuEmitter::ParticleParameters& dst, const ParticleParams& src);

#ifdef USE_IMGUI
	bool DrawImGui(ParticleParams& params);
#endif

} // namespace YScaleOverLifeModule
