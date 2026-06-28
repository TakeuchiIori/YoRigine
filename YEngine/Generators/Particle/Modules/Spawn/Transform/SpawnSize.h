#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 一様スケール（XYZ 同値）のランダム設定
/// 用途: 粒子サイズのランダム化（異方性なし）
/// </summary>
class SpawnSize : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "サイズ"; }
    std::string GetTypeName() const override { return "SpawnSize"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    float minSize_ = 0.5f;
    float maxSize_ = 1.5f;
};
REGISTER_SPAWN_MODULE(SpawnSize)
