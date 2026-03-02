#include "MovingState.h"
#include "../Player.h"

/// <summary>
/// コンストラクタ
/// </summary>
/// <param name="movement">プレイヤー移動クラス</param>
MovingState::MovingState(PlayerMovement* movement) : movement_(movement) {
	// 特別な初期化なし
}

/// <summary>
/// Moving状態に入った時の処理
/// </summary>
void MovingState::OnEnter() {
	//---------------------------------------------------------------------------------------------
	// 初期設定
	//---------------------------------------------------------------------------------------------
	wasRunning_ = false;

	auto* player = movement_->GetOwner();
	auto* combat = player->GetCombat();

	// 死亡中ならアニメーション変更しない
	if (combat && combat->GetCurrentState() == CombatState::Dead) return;

	// コンボ中でなければ歩行モーションを再生
	if (combat && combat->IsIdle()) {
		auto* obj = player->GetObject3d();
		obj->SetMotionSpeed(player->GetMotionSpeed(0));
		obj->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Walk1");
	}
}

/// <summary>
/// Moving状態を抜ける時の処理
/// </summary>
void MovingState::OnExit() {
	wasRunning_ = false;
}

/// <summary>
/// 毎フレーム更新処理
/// </summary>
/// <param name="deltaTime">経過時間</param>
void MovingState::Update(float deltaTime) {
	//---------------------------------------------------------------------------------------------
	// 入力取得・検出
	//---------------------------------------------------------------------------------------------
	InputState input = movement_->GetInputState();
	movement_->DetectInputType(input);

	//---------------------------------------------------------------------------------------------
	// 移動が許可されていない場合の処理
	//---------------------------------------------------------------------------------------------
	if (!movement_->CanMove()) {
		// 速度を0にして停止
		movement_->GetVelocityRef() = Vector3(0.0f, 0.0f, 0.0f);
		// Idle状態に戻る
		machine_->ChangeState(MovementState::Idle);
		return;
	}

	// 入力がなければIdleへ戻す
	if (input.moveDirection.Length() < 0.1f) {
		machine_->ChangeState(MovementState::Idle);
		return;
	}

	//---------------------------------------------------------------------------------------------
	// 移動方向の決定（カメラ相対対応）
	//---------------------------------------------------------------------------------------------
	Vector3 moveDirection = input.moveDirection;
	if (movement_->GetConfig().enableCameraRelativMovement) {
		Vector3 cameraRotation = movement_->GetCameraRotation();
		moveDirection = movement_->CameraMoveDir(input.moveDirection, cameraRotation);
	}

	//---------------------------------------------------------------------------------------------
	// 歩行／走行判定
	//---------------------------------------------------------------------------------------------
	bool isRunning = input.runPressed && movement_->GetConfig().enableRun;

	// 歩行 ↔ 走行 の切り替え時にアニメーション変更
	if (isRunning != wasRunning_) {
		auto* player = movement_->GetOwner();
		auto* combat = player->GetCombat();

		if (combat && combat->GetCurrentState() == CombatState::Dead) {
			wasRunning_ = isRunning;
			return;
		}

		if (combat && combat->IsIdle()) {
			auto* obj = player->GetObject3d();
			obj->SetMotionSpeed(player->GetMotionSpeed(0));

			if (isRunning) obj->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Run1");
			else           obj->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Walk1");
		}

		wasRunning_ = isRunning;
	}

	//---------------------------------------------------------------------------------------------
	// アナログ入力対応（速度スケーリング）
	//---------------------------------------------------------------------------------------------
	if (input.isAnalogInput && movement_->GetConfig().enableAnalogMovement) {
		float speed = isRunning ? movement_->GetConfig().runSpeed : movement_->GetConfig().walkSpeed;
		float speedMultiplier = input.analogMagnitude;

		Vector3 normalizedDirection = (moveDirection.Length() > 0.0f)
			? moveDirection.Normalize() : Vector3{ 0.0f, 0.0f, 0.0f };

		movement_->GetVelocityRef() = normalizedDirection * speed * speedMultiplier;
		movement_->GetTargetDirectionRef() = normalizedDirection;
	} else {
		//-----------------------------------------------------------------------------------------
		// デジタル移動（通常キー入力）
		//-----------------------------------------------------------------------------------------
		float speed = isRunning ? movement_->GetConfig().runSpeed : movement_->GetConfig().walkSpeed;
		if (moveDirection.Length() > 0.0f) {
			Vector3 normalizedDirection = moveDirection.Normalize();
			movement_->GetVelocityRef() = normalizedDirection * speed;
			movement_->GetTargetDirectionRef() = normalizedDirection;
		}
	}

	//---------------------------------------------------------------------------------------------
	// カメラ回転中でも移動方向と回転を更新
	//---------------------------------------------------------------------------------------------
	if (movement_->CanRotate()) {
		movement_->UpdateRotate(deltaTime, movement_->GetTargetDirectionRef());
	}

	//---------------------------------------------------------------------------------------------
	// 移動と回転の適用
	//---------------------------------------------------------------------------------------------
	movement_->ApplyMovement(deltaTime);
	movement_->ApplyRotate();
}