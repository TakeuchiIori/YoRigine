#include "CurveAttackState.h"

#include "../Spacing/SpacingSelector.h"

void CurveAttackState::SetAttack(const EnemyAttack *attack) {
	attack_ = attack;
	name_ = attack ? ("Attack:" + attack->id) : "Attack:none";
}

// ============================================================
// 開始
// ============================================================
void CurveAttackState::Enter(BattleEnemy &enemy) {
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();
	appliedInvincible_ = false;

	// 開始時に一度だけ相手を向く。以降の向きはカーブとモディファイアが決める。
	enemy.FacePlayer();

	player_.Play(enemy, attack_);
}

// ============================================================
// 更新
// ============================================================
void CurveAttackState::Update(BattleEnemy &enemy, float dt) {
	if (!attack_) {
		enemy.ChangeState(SpacingSelector::SelectAfterAttack(enemy));
		return;
	}

	player_.Update(enemy, dt);

	// 無敵はモディファイアの区間に追従させる。
	// このステートが付けた分だけを戻すので、他所で付いた無敵は触らない。
	const bool shouldBeInvincible = player_.IsInvincibleNow();
	if (shouldBeInvincible && !appliedInvincible_) {
		enemy.IsInvincible() = true;
		appliedInvincible_ = true;
	} else if (!shouldBeInvincible && appliedInvincible_) {
		enemy.IsInvincible() = false;
		appliedInvincible_ = false;
	}

	if (player_.IsFinished()) {
		enemy.ChangeState(SpacingSelector::SelectAfterAttack(enemy));
	}
}

// ============================================================
// 終了
//
// 位置は攻撃結果として残し、回転とスケールだけ基準へ戻す。
// 位置まで戻すと突進で詰めた距離が無かったことになってしまう。
// ============================================================
void CurveAttackState::Exit(BattleEnemy &enemy) {
	enemy.SetCanAct(true);

	if (appliedInvincible_) {
		enemy.IsInvincible() = false;
		appliedInvincible_ = false;
	}

	player_.StopKeepPosition(enemy);
}
