#pragma once

#include "../StateMachine.h"
#include "../Combat/PlayerCombat.h"

// ============================================================
// ガードステートクラス
// プレイヤーがガード行動を行っている最中の状態遷移とアニメーションを管理する
// ============================================================
class GuardingCombatState : public IState<CombatState> {
public:
	GuardingCombatState(PlayerCombat* combat);
	~GuardingCombatState() = default;

	void OnEnter() override;
	void OnExit() override;
	void Update(float deltaTime) override;
	CombatState GetStateType() const override { return CombatState::Guarding; }

private:
	PlayerCombat* combat_;
};