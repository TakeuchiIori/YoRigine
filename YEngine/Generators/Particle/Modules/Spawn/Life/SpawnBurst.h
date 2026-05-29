#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 寿命をバースト用に短く設定（0.1〜1 秒程度）
/// SpawnTangentVelocity / SpawnRing と組み合わせて瞬間的な爆発エフェクトに
/// 用途: 着弾エフェクト、攻撃ヒット
/// </summary>
class SpawnBurst : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "バースト寿命"; }
    std::string GetTypeName() const override { return "SpawnBurst"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    float minLife_ = 0.2f;
    float maxLife_ = 0.6f;
};
REGISTER_SPAWN_MODULE(SpawnBurst)
