#pragma once
// ===========================================================================
// YVelocityModule
//
// 初速・速度乱数幅・重力。重力は「速度に働きかける物理量」としてこのモジュールが
// 担当する（Update CS 側でも velocity.y -= gravity*dt という形で速度に作用する）。
// ===========================================================================
#include <GPUParticle/YGpuEmitter.h>
#include <GPUParticle/GpuParticleParams.h>

namespace YVelocityModule {

	void WriteTo(YGpuEmitter::ParticleParameters& dst, const ParticleParams& src);

#ifdef USE_IMGUI
	bool DrawImGui(ParticleParams& params);
#endif

} // namespace YVelocityModule
