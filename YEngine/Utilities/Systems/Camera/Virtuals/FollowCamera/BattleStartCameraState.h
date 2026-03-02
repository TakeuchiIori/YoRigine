#pragma once
#include "CinematicCameraState.h"

/// <summary>
/// 戦闘開始時のカメラワーク
/// </summary>
class BattleStartCameraState : public CinematicCameraState {
public:
    void Enter(FollowCamera* camera) override;
    const char* GetStateName() const override { return "BattleStart"; }
    
    // ImGuiでの編集
    void DrawEditGui() override;
    
private:
    void SetupControlPoints(FollowCamera* camera);
};
