#pragma once

#include "../StateMachine.h"
#include "../Combat/PlayerCombat.h"

class Player;

// ============================================================
// 死亡ステートクラス
// HPが0になった際のモーション再生やゲーム内時間のスロー演出を管理する
// ============================================================
class DeadCombatState : public IState<CombatState> {
public:
	DeadCombatState(PlayerCombat* combat);
	~DeadCombatState() = default;

	void OnEnter() override;
	void OnExit() override;
	void Update(float deltaTime) override;
	CombatState GetStateType() const override { return CombatState::Dead; }

private:
	PlayerCombat* combat_ = nullptr;
	Player* player_ = nullptr;
	float deathTimer_ = 0.0f;
	bool isAnimationFinished_ = false;
};