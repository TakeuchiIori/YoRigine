#include "DeadCombatState.h"
#include "../Player.h"
#include "../Movement/PlayerMovement.h"
#include "Systems/GameTime/GameTime.h"

// ============================================================
// コンストラクタ
// ============================================================
DeadCombatState::DeadCombatState(PlayerCombat* combat) : combat_(combat) {
}

// ============================================================
// 死亡ステート開始処理
// 死亡アニメーションの再生と操作の無効化を行う
// ============================================================
void DeadCombatState::OnEnter() {
	auto* player = combat_->GetOwner();
	auto* obj = player->GetObject3d();

	// ------------------------------------------------------------
	// ゲーム全体の時間進行を減速させる演出
	// ------------------------------------------------------------
	YoRigine::GameTime::SetTimeScale(0.75f);

	// ------------------------------------------------------------
	// 死亡アニメーションの設定
	// ------------------------------------------------------------
	obj->SetMotionSpeed(player->GetMotionSpeed(3));
	obj->SetChangeMotion("Player.gltf", MotionPlayMode::Once, "Death2");

	// ------------------------------------------------------------
	// プレイヤーの操作入力を無効化し、移動を止める
	// ------------------------------------------------------------
	player->GetMovement()->SetCanMove(false);
	player->GetMovement()->SetCanRotate(false);
	player->GetMovement()->ForceStop();

	deathTimer_ = 0.0f;
	isAnimationFinished_ = false;

	combat_->NotifyAction("プレイヤー死亡");
}

// ============================================================
// 死亡ステート終了処理（復活やリトライ時用）
// ============================================================
void DeadCombatState::OnExit() {
	YoRigine::GameTime::SetTimeScale(1.0f);
}

// ============================================================
// 更新処理
// 死亡アニメーションの終了を監視する
// ============================================================
void DeadCombatState::Update(float deltaTime) {
	deathTimer_ += deltaTime;

	// ------------------------------------------------------------
	// 死亡モーションの再生終了を検出
	// ------------------------------------------------------------
	if (!isAnimationFinished_) {
		auto* player = combat_->GetOwner();
		auto* obj = player->GetObject3d();

		if (obj->GetModel() && obj->GetModel()->GetMotionSystem()) {
			auto* motionSystem = obj->GetModel()->GetMotionSystem();

			if (motionSystem->IsFinished()) {
				YoRigine::GameTime::Pause();
				isAnimationFinished_ = true;
			}
		}
	}
}