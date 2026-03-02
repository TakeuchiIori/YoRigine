#pragma once
#include "CameraState.h"

/// <summary>
/// デフォルトステート：通常の追従カメラ
/// </summary>
class DefaultCameraState : public CameraState {
public:
    void Enter(FollowCamera* camera) override;
    void Update(FollowCamera* camera) override;
    void Exit(FollowCamera* camera) override;
    
    const char* GetStateName() const override { return "Default"; }
};
