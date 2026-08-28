#pragma once

#include "../../../Attack/Runtime/AttackPlayer.h"
#include "../../../IEnemyState.h"
#include "../../BattleEnemy.h"

#include <string>

// ============================================================
// カーブ定義の攻撃を実行するステート
//
// Rush / ChargeRush / Spin / Jump / Combo / Counter の6クラスが
// やっていた「時間でフェーズを切り替える」処理を、
// AttackPlayer への委譲だけに置き換えたもの。
//
// どんな攻撃になるかは EnemyAttack（カーブとモディファイア）が決めるので、
// このクラスは攻撃の種類を一切知らない。
// ============================================================
class CurveAttackState : public IEnemyState<BattleEnemy> {
public:
	// Enter の前に実行する攻撃を渡す。データの寿命は保管庫が保証する。
	void SetAttack(const EnemyAttack *attack);

	void Enter(BattleEnemy &enemy) override;
	void Update(BattleEnemy &enemy, float dt) override;
	void Exit(BattleEnemy &enemy) override;

	const char *GetName() const override { return name_.c_str(); }

	bool IsAttacking() const override { return true; }
	bool IsContactDamageActive() const override { return GetContactDamageWindow() >= 0; }

	// 攻撃判定はモディファイアが決める
	int GetContactDamageWindow() const override { return player_.GetActiveDamageWindow(); }

	bool CanBeParried() const override { return attack_ && attack_->parriable; }

private:
	const EnemyAttack *attack_ = nullptr;
	AttackPlayer player_;

	// 無敵をこのステートで付けたかどうか。
	// 攻撃前から無敵だった場合に、終了時へ勝手に解除しないため。
	bool appliedInvincible_ = false;

	std::string name_ = "Attack";
};
