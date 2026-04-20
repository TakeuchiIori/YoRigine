#pragma once

#include "../StateMachine.h"
#include "../Combat/PlayerCombat.h"

// ============================================================
// 攻撃ステートクラス
// 攻撃中における移動制限やエフェクト制御を担当する
// ============================================================
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