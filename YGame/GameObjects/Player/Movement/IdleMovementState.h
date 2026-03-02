#pragma once

#include "../StateMachine.h"
#include "PlayerMovement.h"

/// <summary>
/// 待機移動状態
/// </summary>
class IdleMovementState : public IState<MovementState> {
public:
	IdleMovementState(PlayerMovement* movement);
	~IdleMovementState() = default;

	void OnEnter() override;
	void OnExit() override;
	void Update(float deltaTime) override;
	MovementState GetStateType() const override { return MovementState::Idle; }

private:
	PlayerMovement* movement_;
};