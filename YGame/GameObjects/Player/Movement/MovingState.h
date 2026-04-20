#pragma once

#include "../StateMachine.h"
#include "PlayerMovement.h"

// ============================================================
// 移動ステートクラス
// プレイヤーが入力によって移動(歩行・走行)している状態の処理を行う
// ============================================================
class MovingState : public IState<MovementState> {
public:
	MovingState(PlayerMovement* movement);
	~MovingState() = default;

	void OnEnter() override;
	void OnExit() override;
	void Update(float deltaTime) override;
	MovementState GetStateType() const override { return MovementState::Moving; }

private:
	PlayerMovement* movement_;
	bool wasRunning_ = false;  // 前フレームで走行していたかどうか
};