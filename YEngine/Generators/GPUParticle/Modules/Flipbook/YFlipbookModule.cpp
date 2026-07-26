#include "YFlipbookModule.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace YFlipbookModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const FlipbookParams& src)
	{
		// 0除算・0コマを避けるため下限1でクランプしてから送る
		dst.flipbookCols = (src.cols < 1u) ? 1u : src.cols;
		dst.flipbookRows = (src.rows < 1u) ? 1u : src.rows;
		dst.flipbookFps = src.fps;
		dst.flipbookEnable = src.isEnable ? 1u : 0u;
	}

#ifdef USE_IMGUI
	bool DrawImGui(FlipbookParams& params)
	{
		bool changed = false;

		int cols = static_cast<int>(params.cols);
		int rows = static_cast<int>(params.rows);
		if (ImGui::DragInt("横のコマ数", &cols, 1, 1, 32)) { params.cols = static_cast<uint32_t>(cols < 1 ? 1 : cols); changed = true; }
		if (ImGui::DragInt("縦のコマ数", &rows, 1, 1, 32)) { params.rows = static_cast<uint32_t>(rows < 1 ? 1 : rows); changed = true; }

		changed |= ImGui::DragFloat("再生速度 (fps)", &params.fps, 0.5f, 0.0f, 120.0f, "%.1f fps");
		if (params.fps <= 0.0001f) {
			ImGui::TextDisabled("fps=0: 寿命全体でちょうど1周します（爆発の一発再生向き）");
		} else {
			ImGui::TextDisabled("末尾まで再生したらループします（炎など継続表現向き）");
		}
		ImGui::TextDisabled("総コマ数: %d", cols * rows);

		return changed;
	}
#endif

} // namespace YFlipbookModule
