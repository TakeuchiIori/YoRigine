#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 中心から外側に向かう放射状の速度を設定
/// 用途: 爆発、衝撃波、拡散エフェクトなど
/// </summary>
class SpawnRadialVelocity : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName() const override { return "放射状速度"; }
    std::string GetTypeName() const override { return "SpawnRadialVelocity"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    float speed_ = 10.0f;
    float speedVariation_ = 2.0f;
    bool use3D_ = true; // 3D放射 or 2D放射（XZ平面のみ）
};

REGISTER_SPAWN_MODULE(SpawnRadialVelocity)
