#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 2 色間のランダムカラー設定（RGB/Alpha それぞれ独立）
/// SpawnRandomColor と同等だが名前をわかりやすく統一した版
/// 用途: 炎、血しぶき、光の粒など色にバリエーションが欲しいとき
/// </summary>
class SpawnColorRange : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "カラー範囲"; }
    std::string GetTypeName() const override { return "SpawnColorRange"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    Vector4 colorA_ = { 1.0f, 0.8f, 0.2f, 1.0f };
    Vector4 colorB_ = { 1.0f, 0.2f, 0.0f, 0.8f };
};
REGISTER_SPAWN_MODULE(SpawnColorRange)
