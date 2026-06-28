#include "UpdateVelocity.h"

void UpdateVelocity::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{        
	// 速度に基づいて座標を動かすだけの
	if (!isRandom_) {
		attrs[index].velocity = velocity_;
	}
	attrs[index].position += attrs[index].velocity * dt;
}

void UpdateVelocity::DrawEditor()
{
#ifdef USE_IMGUI
	ImGui::Checkbox("ランダム速度", &isRandom_);
	ImGui::DragFloat3("速度", &velocity_.x, 0.1f);
#endif
}

void UpdateVelocity::SaveToJson(nlohmann::json& json) const
{
	json = {
		{"isRandom", isRandom_},
		{"velocity", {
			{"x", velocity_.x},
			{"y", velocity_.y},
			{"z", velocity_.z}
		}}
	};
}

void UpdateVelocity::LoadFromJson(const nlohmann::json& json)
{
	if (json.contains("isRandom")) {
		isRandom_ = json["isRandom"];
	}
	if (json.contains("velocity")) {
		velocity_.x = json["velocity"]["x"];
		velocity_.y = json["velocity"]["y"];
		velocity_.z = json["velocity"]["z"];
	}
}
