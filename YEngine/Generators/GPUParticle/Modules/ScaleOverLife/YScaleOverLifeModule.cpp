#include "YScaleOverLifeModule.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace YScaleOverLifeModule {

	void WriteTo(YGpuEmitter::ParticleParameters& dst, const ParticleParams& src)
	{
		dst.startScale = src.startScale;
		dst.startScaleVariance = src.startScaleVariance;
		dst.endScale = src.endScale;
		dst.endScaleVariance = src.endScaleVariance;
	}

#ifdef USE_IMGUI
	bool DrawImGui(ParticleParams& params)
	{
		bool changed = false;

		changed |= ImGui::DragFloat3("開始スケール", &params.startScale.x,
			0.01f, 0.01f, 100.0f, "%.2f");
		changed |= ImGui::DragFloat3("開始ランダムスケール幅", &params.startScaleVariance.x,
			0.01f, 0.0f, 50.0f, "± %.2f");
		ImGui::Spacing();

		changed |= ImGui::DragFloat3("終了スケール", &params.endScale.x,
			0.01f, 0.0f, 100.0f, "%.2f");
		changed |= ImGui::DragFloat3("終了ランダムスケール幅", &params.endScaleVariance.x,
			0.01f, 0.0f, 50.0f, "± %.2f");

		return changed;
	}
#endif

} // namespace YScaleOverLifeModule
