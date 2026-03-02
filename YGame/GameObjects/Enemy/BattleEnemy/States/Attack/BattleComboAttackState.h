#pragma once
#include "../../../IEnemyState.h"
#include "../../BattleEnemy.h"
#include "../BattleIdleState.h"

/// <summary>
/// コンボ攻撃状態（短い突進を3回連続）
/// </summary>
class BattleComboAttackState : public IEnemyState<BattleEnemy> {
public:
	void Enter(BattleEnemy& enemy) override;

	void Update(BattleEnemy& enemy, float dt) override;

	void Exit(BattleEnemy& enemy) override;

private:
	// --- 補助関数 (Helper Functions) ---
	// プレイヤーの方向を向く
	void UpdateOrientation(BattleEnemy& enemy);
	// 突進移動を実行する
	void ExecuteRush(BattleEnemy& enemy, float speedMultiplier, float dt);
private:
	int comboCount_ = 0;
	Vector3 anticipationStartPos_{};
	bool hasPerformedAnticipation_ = false;
};