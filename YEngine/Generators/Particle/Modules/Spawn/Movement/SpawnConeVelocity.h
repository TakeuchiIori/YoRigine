#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// コーン（円錐）状に速度を設定
/// 用途: スパーク、爆発、噴射エフェクトなど
/// </summary>
class SpawnConeVelocity : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName() const override { return "コーン速度"; }
    std::string GetTypeName() const override { return "SpawnConeVelocity"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    Vector3 direction_ = { 0.0f, 1.0f, 0.0f };  // コーンの中心方向
    float coneAngle_ = 30.0f;   // コーンの角度（度数法）
    float speed_ = 5.0f;        // 速度の大きさ
    float speedVariation_ = 2.0f; // 速度のランダム幅
};

REGISTER_SPAWN_MODULE(SpawnConeVelocity)
