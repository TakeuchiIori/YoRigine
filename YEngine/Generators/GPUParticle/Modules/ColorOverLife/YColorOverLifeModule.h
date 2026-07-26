#pragma once
// ===========================================================================
// YColorOverLifeModule
//
// 開始/終了カラー（RGBA + 乱数幅）。Age基準の補間はVS側で導出、ここはCBVへの
// 値供給のみ担当。
// ===========================================================================
#include <GPUParticle/YGpuEmitter.h>
#include <GPUParticle/GpuParticleParams.h>

namespace YColorOverLifeModule {

	void WriteTo(YGpuEmitter::ParticleParameters& dst, const ParticleParams& src);

#ifdef USE_IMGUI
	bool DrawImGui(ParticleParams& params);
#endif

} // namespace YColorOverLifeModule
