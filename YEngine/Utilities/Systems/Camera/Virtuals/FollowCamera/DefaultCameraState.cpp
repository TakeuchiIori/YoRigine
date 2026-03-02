#include "DefaultCameraState.h"
#include "FollowCamera.h"
#include <Systems/Input/Input.h>

void DefaultCameraState::Enter([[maybe_unused]]  FollowCamera* camera) {
    stateTimer_ = 0.0f;
}

void DefaultCameraState::Update(FollowCamera* camera) {
    stateTimer_ += 0.016f; // デルタタイム想定
    
    // 通常の追従処理
    camera->UpdateInput();
    camera->FollowProcess();
}

void DefaultCameraState::Exit([[maybe_unused]]  FollowCamera* camera) {
    // 特に何もしない
}
