#include "YDragModule.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace YDragModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const DragParams& src)
	{
		dst.dragCoefficient = src.coefficient;
		dst.dragEnable = src.isEnable ? 1u : 0u;
	}

#ifdef USE_IMGUI
	bool DrawImGui(DragParams& params)
	{
		bool changed = false;
		changed |= ImGui::DragFloat("抵抗係数 (1/秒)", &params.coefficient, 0.05f, 0.0f, 20.0f, "%.2f");
		ImGui::TextDisabled("大きいほど早く失速します。破片の爆散なら 2〜5 程度が目安");
		return changed;
	}
#endif

} // namespace YDragModule
