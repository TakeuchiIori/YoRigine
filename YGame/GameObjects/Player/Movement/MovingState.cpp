#include "MovingState.h"
#include "../Player.h"

// ============================================================
// コンストラクタ
// ============================================================
MovingState::MovingState(PlayerMovement* movement) : movement_(movement) {
}

// ============================================================
// 移動ステート開始処理
// ============================================================
void MovingState::OnEnter() {
	// ------------------------------------------------------------
	// 初期設定
	// ------------------------------------------------------------
	wasRunning_ = false;

	auto* player = movement_->GetOwner();
	auto* combat = player->GetCombat();

	// 全身を占有するアクション中（死亡・被弾・ガード・スタン）はスキップ
	if (combat && combat->GetCurrentState() == CombatState::Dead) return;

	// 全身占有アクション中でなければ歩行モーションを再生
	if (!combat || !combat->IsFullBodyAction()) {
		auto* obj = player->GetObject3d();
		obj->SetMotionSpeed(player->GetMotionSpeed(0));
		obj->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Walk1");
	}
}

// ============================================================
// 移動ステート終了処理
// ============================================================
void MovingState::OnExit() {
	wasRunning_ = false;
}

// ============================================================
// 更新処理
// ============================================================
void MovingState::Update(float deltaTime) {
	// ------------------------------------------------------------
	// 入力取得・検出
	// ------------------------------------------------------------
	InputState input = movement_->GetInputState();
	movement_->DetectInputType(input);

	// ------------------------------------------------------------
	// 移動が許可されていない場合の処理
	// ------------------------------------------------------------
	if (!movement_->CanMove()) {
		movement_->GetVelocityRef() = Vector3(0.0f, 0.0f, 0.0f);
		machine_->ChangeState(MovementState::Idle);
		return;
	}

	// 入力がなければIdleへ戻す
	if (input.moveDirection.Length() < 0.1f) {
		machine_->ChangeState(MovementState::Idle);
		return;
	}

	// ------------------------------------------------------------
	// 移動方向の決定（カメラ相対対応）
	// ------------------------------------------------------------
	Vector3 moveDirection = input.moveDirection;
	if (movement_->GetConfig().enableCameraRelativMovement) {
		Vector3 cameraRotation = movement_->GetCameraRotation();
		moveDirection = movement_->CameraMoveDir(input.moveDirection, cameraRotation);
	}

	// ------------------------------------------------------------
	// 歩行／走行判定
	// ------------------------------------------------------------
	bool isRunning = input.runPressed && movement_->GetConfig().enableRun;

	if (isRunning != wasRunning_) {
		auto* player = movement_->GetOwner();
		auto* combat = player->GetCombat();

		if (combat && combat->GetCurrentState() == CombatState::Dead) {
			wasRunning_ = isRunning;
			return;
		}

		// 全身占有アクション中でなければアニメーション切り替え
		if (!combat || !combat->IsFullBodyAction()) {
			auto* obj = player->GetObject3d();
			obj->SetMotionSpeed(player->GetMotionSpeed(0));

			if (isRunning) obj->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Run1");
			else           obj->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Walk1");
		}

		wasRunning_ = isRunning;
	}

	// ------------------------------------------------------------
	// アナログ入力対応（速度スケーリング）
	// ------------------------------------------------------------
	if (input.isAnalogInput && movement_->GetConfig().enableAnalogMovement) {
		float speed = isRunning ? movement_->GetConfig().runSpeed : movement_->GetConfig().walkSpeed;
		float speedMultiplier = input.analogMagnitude;

		Vector3 normalizedDirection = (moveDirection.Length() > 0.0f)
			? moveDirection.Normalize() : Vector3{ 0.0f, 0.0f, 0.0f };

		movement_->GetVelocityRef() = normalizedDirection * speed * speedMultiplier;
		movement_->GetTargetDirectionRef() = normalizedDirection;
	}
	else {
		// ------------------------------------------------------------
		// デジタル移動（通常キー入力）
		// ------------------------------------------------------------
		float speed = isRunning ? movement_->GetConfig().runSpeed : movement_->GetConfig().walkSpeed;
		if (moveDirection.Length() > 0.0f) {
			Vector3 normalizedDirection = moveDirection.Normalize();
			movement_->GetVelocityRef() = normalizedDirection * speed;
			movement_->GetTargetDirectionRef() = normalizedDirection;
		}
	}

	// ------------------------------------------------------------
	// カメラ回転中でも移動方向と回転を更新
	// ------------------------------------------------------------
	if (movement_->CanRotate()) {
		movement_->UpdateRotate(deltaTime, movement_->GetTargetDirectionRef());
	}

	// ------------------------------------------------------------
	// 移動と回転の適用
	// ------------------------------------------------------------
	movement_->ApplyMovement(deltaTime);
	movement_->ApplyRotate();
}