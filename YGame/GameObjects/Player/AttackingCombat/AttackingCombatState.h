#pragma once

#include "../StateMachine.h"
#include "../Combat/PlayerCombat.h"

// ============================================================
// 攻撃ステートクラス
// 攻撃中における移動制限、アニメーションの進行、当たり判定、
// およびプレイヤーの先行入力（キャンセル）を管理する
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
	float stateTimer_ = 0.0f; // 攻撃アニメーションの経過時間
};