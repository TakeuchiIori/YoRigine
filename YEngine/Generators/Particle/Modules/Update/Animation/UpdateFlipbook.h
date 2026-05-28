#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// テクスチャシートアニメーション（フリップブック）
/// SpawnSubTexture で初期フレームを設定し、このモジュールでアニメーション
/// 用途: 炎・煙・爆発・水しぶきのパラパラアニメ
/// </summary>
class UpdateFlipbook : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "フリップブック"; }
    std::string GetTypeName() const override { return "UpdateFlipbook"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    int   cols_    = 4;      // テクスチャの列数
    int   rows_    = 4;      // テクスチャの行数
    float fps_     = 12.0f;  // 再生速度 (frames/sec)
    bool  loopEnd_ = false;  // 最終フレームで消滅するか
};
REGISTER_UPDATE_MODULE(UpdateFlipbook)
