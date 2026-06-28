#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// テクスチャアトラス（スプライトシート）の初期フレームをランダム設定
/// UpdateFlipbook と組み合わせてアニメーション開始フレームをばらけさせる
/// 用途: 炎・煙・爆発のパラパラアニメ
/// </summary>
class SpawnSubTexture : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "サブテクスチャ"; }
    std::string GetTypeName() const override { return "SpawnSubTexture"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    int cols_       = 4;   // シート列数
    int rows_       = 4;   // シート行数
    bool randomStart_ = true; // ランダム開始フレーム
};
REGISTER_SPAWN_MODULE(SpawnSubTexture)
