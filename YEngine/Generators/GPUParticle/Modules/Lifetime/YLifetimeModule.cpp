#include "YLifetimeModule.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace YLifetimeModule {

	void WriteTo(YGpuEmitter::ParticleParameters& dst, const ParticleParams& src)
	{
		dst.lifeTime = src.lifeTime;
		dst.lifeTimeVariance = src.lifeTimeVariance;
	}

#ifdef USE_IMGUI
	bool DrawImGui(ParticleParams& params)
	{
		bool changed = false;

		changed |= ImGui::DragFloat("基本時間 (秒)", &params.lifeTime,
			0.1f, 0.1f, 30.0f, "%.2f 秒");

		changed |= ImGui::DragFloat("ランダム生存幅 (±)", &params.lifeTimeVariance,
			0.01f, 0.0f, 10.0f, "± %.2f 秒");

		float minLife = params.lifeTime - params.lifeTimeVariance;
		float maxLife = params.lifeTime + params.lifeTimeVariance;

		ImGui::BeginDisabled();
		ImGui::Text("範囲: %.2f ~ %.2f 秒", minLife, maxLife);
		ImGui::EndDisabled();

		return changed;
	}
#endif

} // namespace YLifetimeModule
