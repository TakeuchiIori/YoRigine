#pragma once

#include "../StateMachine.h"
#include "../Combat/PlayerCombat.h"

/// <summary>
/// 回避状態
/// </summary>
class DodgingCombatState : public IState<CombatState> {
public:
	DodgingCombatState(PlayerCombat* combat);
	~DodgingCombatState() = default;

	void OnEnter() override;
	void OnExit() override;
	void Update([[maybe_unused]] float deltaTime) override;
	CombatState GetStateType() const override { return CombatState::Dodging; }

private:
	PlayerCombat* combat_;
};