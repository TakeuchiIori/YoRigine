#include "YEmissiveModule.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace YEmissiveModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const EmissiveParams& src)
	{
		dst.emissiveIntensity = src.intensity;
		dst.emissiveEnable = src.isEnable ? 1u : 0u;
	}

#ifdef USE_IMGUI
	bool DrawImGui(EmissiveParams& params)
	{
		bool changed = false;
		changed |= ImGui::DragFloat("輝度倍率", &params.intensity, 0.05f, 0.0f, 50.0f, "%.2f 倍");
		ImGui::TextDisabled("1超でBloomが乗り始めます。発光弾なら 3〜10 程度が目安");
		ImGui::TextDisabled("ブレンドモードが加算(Add)だとより光って見えます");
		return changed;
	}
#endif

} // namespace YEmissiveModule
