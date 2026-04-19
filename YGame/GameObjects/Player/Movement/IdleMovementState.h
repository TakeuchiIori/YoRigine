#pragma once

#include "../StateMachine.h"
#include "PlayerMovement.h"

// ============================================================
// 待機移動ステートクラス
// プレイヤーの移動入力がない状態の処理を行う
// ============================================================
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