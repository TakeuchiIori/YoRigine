#pragma once

// App
#include "MovementTypes.h"
#include "../StateMachine.h"

// Engine
#include "WorldTransform/WorldTransform.h"
#include <Loaders/Json/JsonManager.h>
// C++
#include <functional>

class Player;

/// <summary>
/// プレイヤーの移動クラス
/// </summary>
class PlayerMovement {
public:
	///************************* 基本的関数 *************************///
	PlayerMovement(Player* owner);

	void Update(float deltaTime);
	void InitJson(YoRigine::JsonManager* jsonManager);

	void ShowStateDebug();

	const char* GetStateString(MovementState state) const;

	///************************* StateMachine関連 *************************///
	void ChangeState(MovementState newState) { stateMachine_.ChangeState(newState); }
	bool CanTransitionTo([[maybe_unused]] MovementState newState) const;

	MovementState GetCurrentState() const { return stateMachine_.GetCurrentState(); }
	MovementState GetPreviousState() const { return stateMachine_.GetPreviousState(); }
	bool StateChanged() const { return stateMachine_.StateChanged(); }

	///************************* コールバック設定 *************************///
	void SetInputTypeChangeCallback(std::function<void(InputType)> callback) {
		onInputTypeChanged_ = callback;
	}

public:
	///************************* アクセッサ *************************///

	bool IsRotating() const { return isRotating_; }
	void SetIsRotating(bool isRotating) { isRotating_ = isRotating; }
	bool CanRotate() const { return canRotate_; }
	void SetCanRotate(bool canRotate) { canRotate_ = canRotate; }

	float GetCurrentRotate() const { return currentRotateY_; }

	Vector3 GetForwardDirection() const;
	Vector3 GetVelocity() const { return velocity_; }
	float GetSpeed() const;
	bool IsMoving() const;
	bool CanMove() const { return canMove_; }  // ← 追加：移動可能かどうかを返す
	void SetCanMove(bool canMove) { canMove_ = canMove; }
	void ForceStop();

	const MovementConfig& GetConfig() const { return config_; }
	InputType GetCurrentInputType() const { return lastInputType_; }

	///************************* State用の公開関数 *************************///
	Player* GetOwner() const { return owner_; }

	// State用の入力処理
	InputState GetInputState() const;
	void DetectInputType(const InputState& input);

	// State用の回転処理
	void UpdateRotate(float deltaTime, const Vector3& moveDirection);
	void ApplyRotate();

	// State用の移動処理
	void ApplyMovement(float deltaTime);

	// 攻撃時の前進ステップ（補間版）
	// 敵の位置に向かって補間で前進を要求する
	void RequestAttackStep(const Vector3& targetPosition, float stepDistance);
	// 補間中かどうか
	bool IsAttackStepping() const { return isAttackStepping_; }

	// 内部データアクセス
	Vector3& GetVelocityRef() { return velocity_; }
	Vector3& GetTargetDirectionRef() { return targetDirection_; }
	float& GetStateTimerRef() { return stateTimer_; }

	// アクセッサに追加
	bool IsRunning() const {
		return GetCurrentState() == MovementState::Moving && velocity_.Length() > config_.walkSpeed + 0.01f;
	}
	// カメラ処理
	Vector3 CameraMoveDir(const Vector3& inputDirection, const Vector3& cameraRotation);
	Vector3 GetCameraRotation() const;

	///************************* カメラ追従関連 *************************///
	void UpdateCameraFollow(float deltaTime);
	bool IsCameraMoving() const { return isCameraMoving_; }

private:
	///************************* 内部処理関数 *************************///
	void InitializeStateMachine();

	///************************* 入力処理 *************************///
	InputState GetKeyboardInput() const;
	InputState GetControllerInput() const;

	///************************* 回転処理 *************************///
	float CalculateTargetRotate(const Vector3& direction) const;
	float LerpAngle(float from, float to, float t) const;

	///************************* その他 *************************///
	float ApplyDeadzone(float value, float deadzone) const;

private:
	///************************* メンバ変数 *************************///
	Player* owner_;

	// StateMachine
	StateMachine<MovementState> stateMachine_;

	// 移動データ
	Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
	Vector3 targetDirection_{ 0.0f, 0.0f, 0.0f };
	MovementConfig config_;

	float currentRotateY_ = 0.0f;
	float targetRotateY_ = 0.0f;
	bool isRotating_ = false;

	// 状態別データ
	float stateTimer_ = 0.0f;

	// 制御フラグ
	bool canMove_ = true;
	bool canRotate_ = true;

	bool enableCameraRelativeMovement_ = true;
	InputType lastInputType_ = InputType::Keyboard;
	float inputSwitchCooldown_ = 0.0f;

	// コールバック
	std::function<void(InputType)> onInputTypeChanged_;

	///************************* カメラ追従用 *************************///
	float previousCameraRotationY_ = 0.0f;          // 前フレームのカメラ回転
	bool isCameraMoving_ = false;                   // カメラが動いているか
	float cameraStopTimer_ = 0.0f;                  // カメラが止まってからの経過時間

	///******* 攻撃ステップ補間用 *//////
	Vector3 stepStartPos_{ 0.0f, 0.0f, 0.0f };     // 補間開始位置
	Vector3 stepTargetPos_{ 0.0f, 0.0f, 0.0f };    // 補間目標位置
	float stepProgress_ = 0.0f;                     // 補間進行率 0.0～1.0
	bool isAttackStepping_ = false;                 // 補間中フラグ
};