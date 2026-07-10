#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"
#include "../../../ParticleCurve.h"

/// <summary>
/// 寿命に応じてスケールを変化させる（最重要モジュールの一つ）
/// initialScale を起点に、N 点の自由カーブ curve_ の値を乗算する。
/// 用途: ほぼ全てのエフェクト（炎が広がって消える、火花が縮む、膨らんで縮む等）
/// </summary>
class UpdateSizeOverLifetime : public IUpdateModule {
public:
    UpdateSizeOverLifetime();
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "寿命でサイズ変化"; }
    std::string GetTypeName() const override { return "UpdateSizeOverLifetime"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    ParticleCurve curve_;  // 寿命[0,1] → スケール倍率
};
REGISTER_UPDATE_MODULE(UpdateSizeOverLifetime)
