#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 生成時にランダムな生存時間を設定
/// 用途: バラバラに消えるエフェクト
/// </summary>
class SpawnLifeTime : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName() const override { return "生存時間"; }
    std::string GetTypeName() const override { return "SpawnLifeTime"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

    // エフェクト再生尺の見積り用（最後の粒はこの秒数まで生存しうる）
    float GetMaxLifetimeHint() const override { return maxLifeTime_; }

private:
    float minLifeTime_ = 0.5f;  // 最小生存時間
    float maxLifeTime_ = 2.0f;  // 最大生存時間
};

REGISTER_SPAWN_MODULE(SpawnLifeTime)