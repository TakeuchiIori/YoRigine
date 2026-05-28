#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 寿命に応じてスケールを変化させる（最重要モジュールの一つ）
/// initialScale を起点に startScale → endScale まで補間する
/// 用途: ほぼ全てのエフェクト（炎が広がって消える、火花が縮む等）
/// </summary>
class UpdateSizeOverLifetime : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "寿命でサイズ変化"; }
    std::string GetTypeName() const override { return "UpdateSizeOverLifetime"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    float startScale_ = 0.0f; // 生成直後の倍率（initialScale × startScale）
    float endScale_   = 0.0f; // 消滅直前の倍率
    bool  useCurve_   = true; // true = smoothstep, false = linear
};
REGISTER_UPDATE_MODULE(UpdateSizeOverLifetime)
