#include "YRotationModule.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace YRotationModule {

	void WriteTo(YGpuEmitter::ParticleParameters& dst, const ParticleParams& src)
	{
		dst.rotation = src.rotation;
		dst.rotationVariance = src.rotationVariance;
		dst.rotationSpeed = src.rotationSpeed;
		dst.rotationSpeedVariance = src.rotationSpeedVariance;
	}

#ifdef USE_IMGUI
	namespace {
		constexpr float kRadToDeg = 180.0f / 3.14159265f;
		constexpr float kDegToRad = 3.14159265f / 180.0f;
	}

	bool DrawImGui(ParticleParams& params)
	{
		bool changed = false;

		float rotationDeg = params.rotation * kRadToDeg;
		if (ImGui::DragFloat("初期回転角度", &rotationDeg, 1.0f, -360.0f, 360.0f, "%.1f°")) {
			params.rotation = rotationDeg * kDegToRad;
			changed = true;
		}

		float rotationVarianceDeg = params.rotationVariance * kRadToDeg;
		if (ImGui::DragFloat("ランダム回転幅", &rotationVarianceDeg, 1.0f, 0.0f, 180.0f, "± %.1f°")) {
			params.rotationVariance = rotationVarianceDeg * kDegToRad;
			changed = true;
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		float rotationSpeedDeg = params.rotationSpeed * kRadToDeg;
		if (ImGui::DragFloat("回転速度", &rotationSpeedDeg, 1.0f, -360.0f, 360.0f, "%.1f°/s")) {
			params.rotationSpeed = rotationSpeedDeg * kDegToRad;
			changed = true;
		}

		float rotationSpeedVarianceDeg = params.rotationSpeedVariance * kRadToDeg;
		if (ImGui::DragFloat("ランダム回転速度幅", &rotationSpeedVarianceDeg, 1.0f, 0.0f, 180.0f, "± %.1f°/s")) {
			params.rotationSpeedVariance = rotationSpeedVarianceDeg * kDegToRad;
			changed = true;
		}

		ImGui::Spacing();
		ImGui::TextDisabled("回転プリセット:");
		if (ImGui::Button("回転しない")) {
			params.rotationSpeed = 0.0f; params.rotationSpeedVariance = 0.0f; changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("ゆっくり右回転")) {
			params.rotationSpeed = 0.5f; params.rotationSpeedVariance = 0.1f; changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("速く右回転")) {
			params.rotationSpeed = 2.0f; params.rotationSpeedVariance = 0.5f; changed = true;
		}
		if (ImGui::Button("ゆっくり左回転")) {
			params.rotationSpeed = -0.5f; params.rotationSpeedVariance = 0.1f; changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("速く左回転")) {
			params.rotationSpeed = -2.0f; params.rotationSpeedVariance = 0.5f; changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("ランダム回転")) {
			params.rotationSpeed = 0.0f; params.rotationSpeedVariance = 2.0f; changed = true;
		}

		ImGui::Spacing();
		ImGui::BeginDisabled();
		float minRotSpeed = (params.rotationSpeed - params.rotationSpeedVariance) * kRadToDeg;
		float maxRotSpeed = (params.rotationSpeed + params.rotationSpeedVariance) * kRadToDeg;
		ImGui::Text("回転速度の範囲: %.1f° ~ %.1f° per second", minRotSpeed, maxRotSpeed);
		ImGui::EndDisabled();

		return changed;
	}
#endif

} // namespace YRotationModule
