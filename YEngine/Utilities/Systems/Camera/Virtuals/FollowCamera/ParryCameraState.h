#pragma once
#include "CinematicCameraState.h"

/// <summary>
/// パリィ成功時のカメラワーク
/// </summary>
class ParryCameraState : public CinematicCameraState {
public:
    void Enter(FollowCamera* camera) override;
    const char* GetStateName() const override { return "Parry"; }
    
    // パリィの演出タイプ
    enum class ParryType {
        Quick,    // 素早いカット
        Dramatic, // ドラマチック
        SlowMotion // スローモーション風
    };
    
    void SetParryType(ParryType type) { parryType_ = type; }
    ParryType GetParryType() const { return parryType_; }
    
    // 保存・読込
    void Save(nlohmann::json& j) const override;
    void Load(const nlohmann::json& j) override;
    
    // ImGuiでの編集
    void DrawEditGui() override;
    
private:
    void SetupControlPoints(FollowCamera* camera);
    ParryType parryType_ = ParryType::Quick;
};
