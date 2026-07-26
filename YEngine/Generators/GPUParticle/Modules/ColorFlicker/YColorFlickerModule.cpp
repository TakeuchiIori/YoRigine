#include "YColorFlickerModule.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace YColorFlickerModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const ColorFlickerParams& src)
	{
		dst.flickerSpeed = src.speed;
		dst.flickerIntensity = src.intensity;
		dst.flickerEnable = src.isEnable ? 1u : 0u;
	}

#ifdef USE_IMGUI
	bool DrawImGui(ColorFlickerParams& params)
	{
		bool changed = false;
		changed |= ImGui::DragFloat("明滅速度 (Hz)", &params.speed, 0.1f, 0.1f, 60.0f, "%.2f Hz");
		changed |= ImGui::DragFloat("明滅の強さ (±%)", &params.intensity, 0.01f, 0.0f, 2.0f, "± %.2f");
		return changed;
	}
#endif

} // namespace YColorFlickerModule
