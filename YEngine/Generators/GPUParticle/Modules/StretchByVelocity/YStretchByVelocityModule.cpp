#include "YStretchByVelocityModule.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace YStretchByVelocityModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const StretchByVelocityParams& src)
	{
		dst.stretchScale = src.scale;
		dst.stretchMax = src.maxStretch;
		dst.stretchEnable = src.isEnable ? 1u : 0u;
	}

#ifdef USE_IMGUI
	bool DrawImGui(StretchByVelocityParams& params)
	{
		bool changed = false;
		changed |= ImGui::DragFloat("伸び率 (速さ1あたり)", &params.scale, 0.01f, 0.0f, 5.0f, "%.3f");
		changed |= ImGui::DragFloat("伸びの上限倍率", &params.maxStretch, 0.1f, 1.0f, 50.0f, "%.2f 倍");
		ImGui::TextDisabled("有効にすると粒子が速度方向を向きます（回転設定は無効になります）");
		return changed;
	}
#endif

} // namespace YStretchByVelocityModule
