#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 指定した方向 + 拡散角で速度を設定するSpawnモジュール
/// 用途: スラッシュエフェクト、弾丸、炎の噴射など
/// </summary>
class SpawnDirection : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "方向速度"; }
    std::string GetTypeName() const override { return "SpawnDirection"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    Vector3 direction_   = { 0.0f, 1.0f, 0.0f }; // 基準方向（正規化済み前提）
    float   spreadAngle_ = 15.0f;   // 拡散角 (度)
    float   minSpeed_    = 3.0f;
    float   maxSpeed_    = 8.0f;
};
REGISTER_SPAWN_MODULE(SpawnDirection)
