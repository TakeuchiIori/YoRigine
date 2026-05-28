#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// XZ 平面のリング状に速度ベクトルを設定（衝撃波、爆発リングに最適）
/// 用途: 着地衝撃波、爆発外縁、魔法陣
/// </summary>
class SpawnRing : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "リング放射"; }
    std::string GetTypeName() const override { return "SpawnRing"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    float minSpeed_    = 3.0f;
    float maxSpeed_    = 6.0f;
    float upwardBias_  = 0.5f;  // 上方向の速度成分
    float radius_      = 1.0f;  // 放射の起点半径
};
REGISTER_SPAWN_MODULE(SpawnRing)
