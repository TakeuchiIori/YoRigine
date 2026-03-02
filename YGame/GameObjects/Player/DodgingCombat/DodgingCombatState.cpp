#include "DodgingCombatState.h"

DodgingCombatState::DodgingCombatState(PlayerCombat* combat) : combat_(combat) {
	// 回避関連のコールバック設定（将来的に実装）
}

void DodgingCombatState::OnEnter() {
	// 回避状態に入った時の処理
	// すぐにIdleに戻る（暫定処理）
	machine_->ChangeState(CombatState::Idle);
}

void DodgingCombatState::OnExit() {
	// 回避状態を抜ける時の処理
}

void DodgingCombatState::Update([[maybe_unused]] float deltaTime) {
	// 回避状態の更新処理
	// TODO: 回避システムが実装されたら、そちらで状態管理
}