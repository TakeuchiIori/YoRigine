#include "SpawnVelocity.h"

void SpawnVelocity::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
	// 生まれた瞬間に、共通のバケツ（velocity）に値を放り込む
	attrs[index].velocity = ParticleMath::RandomVector3(minVel_, maxVel_);
}

void SpawnVelocity::DrawEditor()
{
#ifdef USE_IMGUI
	ImGui::DragFloat3("最低 速度", &minVel_.x, 0.1f);
	ImGui::DragFloat3("最大 速度", &maxVel_.x, 0.1f);
#endif
}

void SpawnVelocity::SaveToJson(nlohmann::json& json) const
{
	json["minVelocity"] = {
		{"x", minVel_.x},
		{"y", minVel_.y},
		{"z", minVel_.z}
	};
	json["maxVelocity"] = {
		{"x",maxVel_.x},
		{"y",maxVel_.y},
		{"z",maxVel_.z}
	};

}

void SpawnVelocity::LoadFromJson(const nlohmann::json& json)
{
	if (json.contains("minVelocity")) {
		minVel_.x = json["minVelocity"]["x"];
		minVel_.y = json["minVelocity"]["y"];
		minVel_.z = json["minVelocity"]["z"];
	}
	if (json.contains("maxVelocity")) {
		maxVel_.x = json["maxVelocity"]["x"];
		maxVel_.y = json["maxVelocity"]["y"];
		maxVel_.z = json["maxVelocity"]["z"];
	}
}
