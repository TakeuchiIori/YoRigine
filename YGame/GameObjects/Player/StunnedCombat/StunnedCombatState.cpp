#include "StunnedCombatState.h"

/// <summary>
/// コンストラクタ
/// スタン状態に関連する設定を初期化（将来的に拡張予定）
/// </summary>
StunnedCombatState::StunnedCombatState(PlayerCombat* combat) : combat_(combat) {
	// スタン関連のコールバック設定（未実装）
}

/// <summary>
/// スタン状態に入った時の処理
/// </summary>
void StunnedCombatState::OnEnter() {
	// TODO: スタン開始時のアニメーションやエフェクトを追加
	// 例：プレイヤーを硬直させる、アニメーション再生、移動ロックなど
}

/// <summary>
/// スタン状態を抜ける時の処理
/// </summary>
void StunnedCombatState::OnExit() {
	// TODO: スタン解除時の処理（移動や操作の再開）
}

/// <summary>
/// 毎フレーム更新処理
/// スタン継続中のタイマー管理や自動復帰などを行う予定
/// </summary>
void StunnedCombatState::Update([[maybe_unused]] float deltaTime) {
	// スタン中の更新処理
	// TODO: スタンタイマーが実装されたら自動でIdleへ戻すなどを追加
}
