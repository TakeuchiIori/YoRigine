#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"


class UpdateVelocity : public IUpdateModule
{
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "速度"; }
    std::string  GetTypeName() const override { return "UpdateVelocity"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
	Vector3 velocity_{ 0.0f, 1.0f, 0.0f };
	bool isRandom_ = false;
};

// ファクトリーに登録
REGISTER_UPDATE_MODULE(UpdateVelocity)