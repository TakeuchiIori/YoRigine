#include "IdleMovementState.h"
#include "../Player.h"

/// <summary>
/// コンストラクタ：IdleMovementState の初期化
/// </summary>
/// <param name="movement">PlayerMovement へのポインタ</param>
IdleMovementState::IdleMovementState(PlayerMovement* movement) : movement_(movement) {
	// コンストラクタでは特に設定なし
}

/// <summary>
/// Idle 状態に入った際の処理
/// </summary>
void IdleMovementState::OnEnter() {
	auto* player = movement_->GetOwner();
	auto* combat = player->GetCombat();

	//---------------------------------------------------------------------------------------------
	// 死亡中・非戦闘時のアニメーション設定
	//---------------------------------------------------------------------------------------------
	if (combat && combat->GetCurrentState() == CombatState::Dead) {
		return;  // 死亡中はアニメーションを変更しない
	}

	if (combat && combat->IsIdle()) {
		auto* obj = player->GetObject3d();
		obj->SetMotionSpeed(player->GetMotionSpeed(0));  // motionSpeed[0]
		obj->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Idle4");
	}

	// 速度リセット
	movement_->GetVelocityRef() = Vector3(0.0f, 0.0f, 0.0f);
}

/// <summary>
/// Idle 状態を抜ける際の処理
/// </summary>
void IdleMovementState::OnExit() {
	// Idle状態を抜ける時の処理（特になし）
}

/// <summary>
/// 毎フレーム更新処理
/// </summary>
/// <param name="deltaTime">経過時間（秒）</param>
void IdleMovementState::Update(float deltaTime) {
	InputState input = movement_->GetInputState();
	movement_->DetectInputType(input);

	//---------------------------------------------------------------------------------------------
	// 入力がある場合、Moving状態へ遷移（移動可能な場合のみ）
	//---------------------------------------------------------------------------------------------
	if (input.moveDirection.Length() > 0.1f) {
		// 移動が許可されている場合のみ状態遷移
		if (movement_->CanMove()) {
			machine_->ChangeState(MovementState::Moving);
			return;
		}
	}

	//---------------------------------------------------------------------------------------------
	// 減速および移動適用処理
	//---------------------------------------------------------------------------------------------
	Vector3& velocity = movement_->GetVelocityRef();
	velocity *= movement_->GetConfig().deceleration;

	// 移動が許可されていない場合は速度を0にする
	if (!movement_->CanMove()) {
		velocity = Vector3(0.0f, 0.0f, 0.0f);
	}

	movement_->ApplyMovement(deltaTime);
	movement_->ApplyRotate();
}