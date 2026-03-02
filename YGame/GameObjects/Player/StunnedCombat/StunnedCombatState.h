#pragma once

#include "../StateMachine.h"
#include "../Combat/PlayerCombat.h"

/// <summary>
/// スタン状態
/// </summary>
class StunnedCombatState : public IState<CombatState> {
public:
	StunnedCombatState(PlayerCombat* combat);
	~StunnedCombatState() = default;

	void OnEnter() override;
	void OnExit() override;
	void Update([[maybe_unused]] float deltaTime) override;
	CombatState GetStateType() const override { return CombatState::Stunned; }

private:
	PlayerCombat* combat_;
};