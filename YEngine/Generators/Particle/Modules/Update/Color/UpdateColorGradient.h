#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"
#include <array>

/// <summary>
/// 複数色を使ったグラデーション
/// 用途: レインボーエフェクト、複雑な色変化など
/// </summary>
class UpdateColorGradient : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "グラデーション"; }
    std::string GetTypeName() const override { return "UpdateColorGradient"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    static const int MAX_COLORS = 5;
    std::array<Vector4, MAX_COLORS> colors_;
    int colorCount_ = 3;
    
    // デフォルトで炎のグラデーション
    void InitializeDefaults() {
        colors_[0] = { 1.0f, 1.0f, 0.0f, 1.0f };  // 黄色
        colors_[1] = { 1.0f, 0.5f, 0.0f, 1.0f };  // オレンジ
        colors_[2] = { 1.0f, 0.0f, 0.0f, 0.0f };  // 赤（透明）
    }
    
public:
    UpdateColorGradient() {
        InitializeDefaults();
    }
};

REGISTER_UPDATE_MODULE(UpdateColorGradient)
