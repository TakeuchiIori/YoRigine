#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 球面接線方向（外向き）に速度を設定（爆発用）
/// 球形エミッタと組み合わせると美しい全方位爆発になる
/// 用途: 爆発、魔法ヒット、破片飛散
/// </summary>
class SpawnTangentVelocity : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "接線速度（爆発）"; }
    std::string GetTypeName() const override { return "SpawnTangentVelocity"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    float minSpeed_ = 2.0f;
    float maxSpeed_ = 8.0f;
};
REGISTER_SPAWN_MODULE(SpawnTangentVelocity)
