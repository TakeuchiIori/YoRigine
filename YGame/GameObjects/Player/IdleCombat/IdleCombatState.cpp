#include "IdleCombatState.h"
#include "../Player.h"
#include "../Movement/PlayerMovement.h"
#include "Systems/GameTime/GameTime.h"

/// <summary>
/// コンストラクタ
/// </summary>
/// <param name="combat">戦闘システムへのポインタ</param>
IdleCombatState::IdleCombatState(PlayerCombat* combat) : combat_(combat) {
	// 特別な初期化は不要
}

/// <summary>
/// 待機状態に入った時の処理
/// </summary>
void IdleCombatState::OnEnter() {
	auto* player = combat_->GetOwner();
	auto* obj = player->GetObject3d();

	//---------------------------------------------------------------------------------------------
	// アイドルモーションを再生
	//---------------------------------------------------------------------------------------------
	obj->SetMotionSpeed(player->GetMotionSpeed(0)); // motionSpeed[0] = 通常速度
	obj->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Idle4");

}

/// <summary>
/// 待機状態を抜ける時の処理
/// </summary>
void IdleCombatState::OnExit() {
	// 特に処理なし
}

/// <summary>
/// 毎フレーム更新処理
/// </summary>
/// <param name="deltaTime">経過時間</param>
void IdleCombatState::Update([[maybe_unused]] float deltaTime) {
	//---------------------------------------------------------------------------------------------
	// アイドル中は特にロジック的な更新は行わない
	//---------------------------------------------------------------------------------------------
	// この状態は「入力待ち」の意味合いが強く、攻撃・ガードなどの行動入力を監視する
}
