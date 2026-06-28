#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 生成時にランダムなカラーを設定
/// 用途: カラフルなエフェクト、火花など
/// </summary>
class SpawnRandomColor : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName() const override { return "ランダム色"; }
    std::string GetTypeName() const override { return "SpawnRandomColor"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    Vector4 minColor_ = { 1.0f, 0.5f, 0.0f, 1.0f }; // オレンジ
    Vector4 maxColor_ = { 1.0f, 1.0f, 0.0f, 1.0f }; // 黄色
};

REGISTER_SPAWN_MODULE(SpawnRandomColor)
