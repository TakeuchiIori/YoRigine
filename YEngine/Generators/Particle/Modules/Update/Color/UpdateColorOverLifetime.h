#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"
#include <array>

/// <summary>
/// 寿命に応じて最大 5 色のグラデーション（Alpha 込み）
/// UpdateColorGradient の改良版（initialColor 基準でブレンド）
/// 用途: 炎（黄→オレンジ→赤→透明）、魔法（白→青→消滅）
/// </summary>
class UpdateColorOverLifetime : public IUpdateModule {
public:
    UpdateColorOverLifetime();
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "寿命でカラー変化"; }
    std::string GetTypeName() const override { return "UpdateColorOverLifetime"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    static const int kMaxStops = 5;
    std::array<float,  kMaxStops> times_;   // 0.0〜1.0
    std::array<Vector4,kMaxStops> colors_;
    int stopCount_ = 3;
};
REGISTER_UPDATE_MODULE(UpdateColorOverLifetime)
