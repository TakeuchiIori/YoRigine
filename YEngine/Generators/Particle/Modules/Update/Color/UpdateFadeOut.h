#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 寿命(age)に応じて透明度をフェードアウト
/// 用途: 煙、火花、爆発など自然に消えるエフェクト
/// </summary>
class UpdateFadeOut : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "フェードアウト"; }
    std::string GetTypeName() const override { return "UpdateFadeOut"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    float fadeStart_ = 0.7f;  // フェード開始タイミング (age 0.7 = 70%の時点)
    float fadeEnd_ = 1.0f;    // フェード終了タイミング (age 1.0 = 100%)
};

REGISTER_UPDATE_MODULE(UpdateFadeOut)