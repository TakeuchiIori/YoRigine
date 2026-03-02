#pragma once

#include "../StateMachine.h"
#include "../Combat/PlayerCombat.h"

class Player;
/// <summary>
/// 死亡状態
/// </summary>
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
	float deathTimer_ = 0.0f;           // 死亡アニメーションの経過時間
	bool isAnimationFinished_ = false;  // アニメーションが終了したか
};