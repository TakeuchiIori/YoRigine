#pragma once

#include "../StateMachine.h"
#include "PlayerMovement.h"

/// <summary>
/// 移動状態（歩行・走行を内包）
/// </summary>
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
	bool wasRunning_ = false;  // 前フレームで走行していたか
};