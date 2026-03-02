#include "UpdateColor.h"

void UpdateColor::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
	(void)dt; // 未使用警告回避

	// dt ではなく、そのパーティクルの現在の寿命割合(0.0~1.0)を使う
	float t = attrs[index].GetNormalizedAge();

	// 0.0〜1.0の範囲にクランプ
	//t = std::max(0.0f, std::min(1.0f, t));

	// 線形補間
	attrs[index].color = Lerp(startColor_, endColor_, t);
}

void UpdateColor::DrawEditor()
{
#ifdef USE_IMGUI
	ImGui::ColorEdit4("開始色", &startColor_.x);
	ImGui::ColorEdit4("終了色", &endColor_.x);
#endif
}

void UpdateColor::SaveToJson(nlohmann::json& json) const
{
	json = {
		{"startColor", {
			{"x", startColor_.x},
			{"y", startColor_.y},
			{"z", startColor_.z},
			{"w", startColor_.w}
		}},
		{"endColor", {
			{"x", endColor_.x},
			{"y", endColor_.y},
			{"z", endColor_.z},
			{"w", endColor_.w}
		}}
	};
}

void UpdateColor::LoadFromJson(const nlohmann::json& json)
{
	if (json.contains("startColor")) {
		startColor_.x = json["startColor"]["x"];
		startColor_.y = json["startColor"]["y"];
		startColor_.z = json["startColor"]["z"];
		startColor_.w = json["startColor"]["w"];
	}

	if (json.contains("endColor")) {
		endColor_.x = json["endColor"]["x"];
		endColor_.y = json["endColor"]["y"];
		endColor_.z = json["endColor"]["z"];
		endColor_.w = json["endColor"]["w"];
	}
}
