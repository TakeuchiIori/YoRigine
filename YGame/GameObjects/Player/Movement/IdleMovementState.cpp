#include "IdleMovementState.h"
#include "../Player.h"

// ============================================================
// コンストラクタ
// ============================================================
IdleMovementState::IdleMovementState(PlayerMovement* movement) : movement_(movement) {
}

// ============================================================
// 待機移動ステート開始処理
// ============================================================
void IdleMovementState::OnEnter() {
	auto* player = movement_->GetOwner();
	auto* combat = player->GetCombat();

	// ------------------------------------------------------------
	// 死亡中・非戦闘時のアニメーション設定
	// ------------------------------------------------------------
	if (combat && combat->GetCurrentState() == CombatState::Dead) {
		return;
	}

	if (combat && combat->IsIdle()) {
		auto* obj = player->GetObject3d();
		obj->SetMotionSpeed(player->GetMotionSpeed(0));
		obj->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Idle4");
	}

	// 速度リセット
	movement_->GetVelocityRef() = Vector3(0.0f, 0.0f, 0.0f);
}

// ============================================================
// 待機移動ステート終了処理
// ============================================================
void IdleMovementState::OnExit() {
}

// ============================================================
// 更新処理
// ============================================================
void IdleMovementState::Update(float deltaTime) {
	InputState input = movement_->GetInputState();
	movement_->DetectInputType(input);

	// ------------------------------------------------------------
	// 入力がある場合、Moving状態へ遷移（移動可能な場合のみ）
	// ------------------------------------------------------------
	if (input.moveDirection.Length() > 0.1f) {
		if (movement_->CanMove()) {
			machine_->ChangeState(MovementState::Moving);
			return;
		}
	}

	// ------------------------------------------------------------
	// 減速および移動適用処理
	// ------------------------------------------------------------
	Vector3& velocity = movement_->GetVelocityRef();
	velocity *= movement_->GetConfig().deceleration;

	// 移動が許可されていない場合は速度を0にする
	if (!movement_->CanMove()) {
		velocity = Vector3(0.0f, 0.0f, 0.0f);
	}

	movement_->ApplyMovement(deltaTime);
	movement_->ApplyRotate();
}