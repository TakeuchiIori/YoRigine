#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 生成時にランダムな角速度を設定（スピン）
/// 用途: 回転する破片、葉、コイン
/// </summary>
class SpawnAngularVelocity : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "角速度"; }
    std::string GetTypeName() const override { return "SpawnAngularVelocity"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    Vector3 minAngVel_ = { -180.0f, -180.0f, -180.0f }; // 度/秒
    Vector3 maxAngVel_ = {  180.0f,  180.0f,  180.0f };
};
REGISTER_SPAWN_MODULE(SpawnAngularVelocity)
