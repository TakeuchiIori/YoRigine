#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"


class UpdateGravity : public IUpdateModule
{
public:
	void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
	void DrawEditor() override;
	std::string GetName() const override { return "重力"; }
	std::string GetTypeName() const override { return "UpdateGravity"; }

	void SaveToJson(nlohmann::json& json) const override;
	void LoadFromJson(const nlohmann::json& json) override;
private:
	Vector3 gravity_ = { 0.0f, -9.81f, 0.0f };
};

// ファクトリーに登録
REGISTER_UPDATE_MODULE(UpdateGravity)