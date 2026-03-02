#pragma once

#include "../StateMachine.h"
#include "../Combat/PlayerCombat.h"

/// <summary>
/// 攻撃状態
/// </summary>
class AttackingCombatState : public IState<CombatState> {
public:
	AttackingCombatState(PlayerCombat* combat);
	~AttackingCombatState() = default;

	void OnEnter() override;
	void OnExit() override;
	void Update(float deltaTime) override;
	CombatState GetStateType() const override { return CombatState::Attacking; }

private:
	PlayerCombat* combat_;
};