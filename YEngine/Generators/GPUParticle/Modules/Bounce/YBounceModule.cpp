#include "YBounceModule.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace YBounceModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const BounceParams& src)
	{
		dst.bounceGroundY = src.groundY;
		dst.bounceRestitution = src.restitution;
		dst.bounceFriction = src.friction;
		dst.bounceEnable = src.isEnable ? 1u : 0u;
	}

#ifdef USE_IMGUI
	bool DrawImGui(BounceParams& params)
	{
		bool changed = false;
		changed |= ImGui::DragFloat("反射面の高さ (ワールドY)", &params.groundY, 0.05f, -100.0f, 100.0f, "%.2f");
		changed |= ImGui::DragFloat("反発係数", &params.restitution, 0.01f, 0.0f, 1.0f, "%.2f");
		changed |= ImGui::DragFloat("接地摩擦", &params.friction, 0.01f, 0.0f, 1.0f, "%.2f");
		ImGui::TextDisabled("水平面での反射のみ。地面コライダーが無いため高さは手動指定です");
		return changed;
	}
#endif

} // namespace YBounceModule
