#include "YVelocityModule.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <cmath>
#endif

namespace YVelocityModule {

	void WriteTo(YGpuEmitter::ParticleParameters& dst, const ParticleParams& src)
	{
		dst.velocity = src.velocity;
		dst.velocityVariance = src.velocityVariance;
		dst.gravity = src.gravity;
	}

#ifdef USE_IMGUI
	bool DrawImGui(ParticleParams& params)
	{
		bool changed = false;

		changed |= ImGui::DragFloat3("基本速度", &params.velocity.x,
			0.01f, -10.0f, 10.0f, "%.2f");
		changed |= ImGui::DragFloat3("ランダム速度幅", &params.velocityVariance.x,
			0.01f, 0.0f, 5.0f, "± %.2f");

		ImGui::Spacing();
		ImGui::TextDisabled("方向プリセット:");
		if (ImGui::Button("上"))     { params.velocity = Vector3(0.0f, 1.0f, 0.0f);  changed = true; }
		ImGui::SameLine();
		if (ImGui::Button("下"))     { params.velocity = Vector3(0.0f, -1.0f, 0.0f); changed = true; }
		ImGui::SameLine();
		if (ImGui::Button("前"))     { params.velocity = Vector3(0.0f, 0.0f, 1.0f);  changed = true; }
		ImGui::SameLine();
		if (ImGui::Button("後ろ"))   { params.velocity = Vector3(0.0f, 0.0f, -1.0f); changed = true; }
		ImGui::SameLine();
		if (ImGui::Button("右"))     { params.velocity = Vector3(1.0f, 0.0f, 0.0f);  changed = true; }
		ImGui::SameLine();
		if (ImGui::Button("左"))     { params.velocity = Vector3(-1.0f, 0.0f, 0.0f); changed = true; }
		ImGui::SameLine();
		if (ImGui::Button("停止"))   { params.velocity = Vector3(0.0f, 0.0f, 0.0f);  changed = true; }

		ImGui::Spacing();
		ImGui::BeginDisabled();
		float speed = std::sqrt(
			params.velocity.x * params.velocity.x +
			params.velocity.y * params.velocity.y +
			params.velocity.z * params.velocity.z
		);
		ImGui::Text("速度の大きさ: %.2f units/sec", speed);
		ImGui::EndDisabled();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		changed |= ImGui::DragFloat("重力影響度", &params.gravity, 0.01f, -10.0f, 10.0f, "%.2f");

		return changed;
	}
#endif

} // namespace YVelocityModule
