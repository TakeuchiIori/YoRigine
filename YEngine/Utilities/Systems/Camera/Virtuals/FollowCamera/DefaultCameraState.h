#pragma once
#include "CameraState.h"

// ============================================================
// デフォルトステート：通常の追従カメラ
// ============================================================
class DefaultCameraState : public CameraState {
public:
	void Enter(FollowCamera* camera) override;
	void Update(FollowCamera* camera) override;
	void Exit(FollowCamera* camera) override;
	bool IsPerformance() const override { return false; }
	const char* GetStateName() const override { return "Default"; }
};