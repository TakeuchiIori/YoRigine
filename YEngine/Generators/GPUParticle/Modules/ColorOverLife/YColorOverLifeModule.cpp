#include "YColorOverLifeModule.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace YColorOverLifeModule {

	void WriteTo(YGpuEmitter::ParticleParameters& dst, const ParticleParams& src)
	{
		dst.startColor = src.startColor;
		dst.startColorVariance = src.startColorVariance;
		dst.endColor = src.endColor;
		dst.endColorVariance = src.endColorVariance;
	}

#ifdef USE_IMGUI
	bool DrawImGui(ParticleParams& params)
	{
		bool changed = false;

		changed |= ImGui::ColorEdit4("開始色", &params.startColor.x,
			ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB);
		changed |= ImGui::DragFloat3("開始色 RGB ランダム幅(±)", &params.startColorVariance.x,
			0.01f, 0.0f, 1.0f, "± %.2f");
		changed |= ImGui::DragFloat("開始色 Alpha ランダム幅 (±)", &params.startColorVariance.w,
			0.01f, 0.0f, 1.0f, "± %.2f");
		ImGui::Spacing();

		changed |= ImGui::ColorEdit4("終了色", &params.endColor.x,
			ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB);
		changed |= ImGui::DragFloat3("終了色 RGB ランダム幅(±)", &params.endColorVariance.x,
			0.01f, 0.0f, 1.0f, "± %.2f");
		changed |= ImGui::DragFloat("終了色 Alpha ランダム幅 (±)", &params.endColorVariance.w,
			0.01f, 0.0f, 1.0f, "± %.2f");

		return changed;
	}
#endif

} // namespace YColorOverLifeModule
