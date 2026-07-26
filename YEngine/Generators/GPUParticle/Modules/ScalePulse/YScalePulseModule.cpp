#include "YScalePulseModule.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace YScalePulseModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const ScalePulseParams& src)
	{
		dst.pulseAmplitude = src.amplitude;
		dst.pulseFrequency = src.frequency;
		dst.pulseEnable = src.isEnable ? 1u : 0u;
	}

#ifdef USE_IMGUI
	bool DrawImGui(ScalePulseParams& params)
	{
		bool changed = false;
		changed |= ImGui::DragFloat("振幅 (±%)", &params.amplitude, 0.01f, 0.0f, 2.0f, "± %.2f");
		changed |= ImGui::DragFloat("周波数 (Hz)", &params.frequency, 0.05f, 0.01f, 20.0f, "%.2f Hz");
		return changed;
	}
#endif

} // namespace YScalePulseModule
