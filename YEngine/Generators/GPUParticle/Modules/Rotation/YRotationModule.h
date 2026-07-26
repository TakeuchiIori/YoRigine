#pragma once
// ===========================================================================
// YRotationModule
//
// 初期回転角・回転速度（度/秒⇔ラジアンの変換もこのモジュールに閉じ込める）。
// ===========================================================================
#include <GPUParticle/YGpuEmitter.h>
#include <GPUParticle/GpuParticleParams.h>

namespace YRotationModule {

	void WriteTo(YGpuEmitter::ParticleParameters& dst, const ParticleParams& src);

#ifdef USE_IMGUI
	bool DrawImGui(ParticleParams& params);
#endif

} // namespace YRotationModule
