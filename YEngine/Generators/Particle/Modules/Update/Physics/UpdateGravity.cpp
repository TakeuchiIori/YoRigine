#include "UpdateGravity.h"
#include "UpdateGravity.h"

void UpdateGravity::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
	// 重力加速度を速度に加算する役割
	attrs[index].velocity += gravity_ * dt;
}

void UpdateGravity::DrawEditor()
{
#ifdef USE_IMGUI
	ImGui::DragFloat("重力", &gravity_.y, 0.1f);
#endif
}

void UpdateGravity::SaveToJson(nlohmann::json& json) const
{
	json = {
		{"gravity", {
			{"x", gravity_.x},
			{"y", gravity_.y},
			{"z", gravity_.z}
		}}
	};
}

void UpdateGravity::LoadFromJson(const nlohmann::json& json)
{
	if (json.contains("gravity")) {
		gravity_.x = json["gravity"]["x"];
		gravity_.y = json["gravity"]["y"];
		gravity_.z = json["gravity"]["z"];
	}
}
