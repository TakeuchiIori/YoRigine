#include "YUVScrollModule.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace YUVScrollModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const UVScrollParams& src)
	{
		dst.uvScrollSpeed = src.scrollSpeed;
		dst.uvScrollEnable = src.isEnable ? 1u : 0u;
	}

#ifdef USE_IMGUI
	bool DrawImGui(UVScrollParams& params)
	{
		bool changed = false;
		changed |= ImGui::DragFloat2("スクロール速度 (UV/秒)", &params.scrollSpeed.x, 0.01f, -5.0f, 5.0f, "%.2f");
		return changed;
	}
#endif

} // namespace YUVScrollModule
