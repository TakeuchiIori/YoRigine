#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"



class SpawnVelocity : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
	std::string GetName() const override { return "速度"; }
	std::string GetTypeName() const override { return "SpawnVelocity"; }

    void SaveToJson(nlohmann::json& json) const override;
	void LoadFromJson(const nlohmann::json& json) override;
private:
    Vector3 minVel_ = { -1, 5, -1 }, maxVel_ = { 1, 10, 1 };
};
// ファクトリーに登録
REGISTER_SPAWN_MODULE(SpawnVelocity)