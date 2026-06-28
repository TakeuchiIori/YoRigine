#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

#include "Vector2.h"

class SpawnUV : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;

    std::string GetName() const override { return "UV初期設定"; }
    std::string GetTypeName() const override { return "SpawnUV"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    Vector2 initialScale = { 1.0f, 1.0f };
    Vector2 initialOffset = { 0.0f, 0.0f };

};

REGISTER_SPAWN_MODULE(SpawnUV)