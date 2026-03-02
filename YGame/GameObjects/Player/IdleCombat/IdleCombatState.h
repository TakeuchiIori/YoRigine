#pragma once

#include "../StateMachine.h"
#include "../Combat/PlayerCombat.h"

/// <summary>
/// 待機状態
/// </summary>
class IdleCombatState : public IState<CombatState> {
public:
	IdleCombatState(PlayerCombat* combat);
	~IdleCombatState() = default;

	void OnEnter() override;
	void OnExit() override;
	void Update([[maybe_unused]] float deltaTime) override;
	CombatState GetStateType() const override { return CombatState::Idle; }

private:
	PlayerCombat* combat_;
};