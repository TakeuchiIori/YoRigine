#pragma once

#include "../StateMachine.h"
#include "../Combat/PlayerCombat.h"

// ============================================================
// スタン戦闘ステートクラス
// 大きなダメージを受けた際など、一定時間行動不能になる状態を管理する
// ============================================================
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