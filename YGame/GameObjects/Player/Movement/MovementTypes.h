#pragma once
#include "Vector3.h"

// 移動状態（シンプル化）
enum class MovementState {
	Idle,      // 待機
	Moving,    // 移動中（歩行・走行を内包）
	Jump,      // ジャンプ
	Stunned    // スタン
};

// 入力タイプ
enum class InputType {
	Keyboard,
	Gamepad,
	Auto
};

// 移動設定
struct MovementConfig {
	float walkSpeed = 0.15f;
	float runSpeed = 0.25f;
	float deceleration = 0.9f;
	bool enableDash = true;
	bool enableRun = true;

	// コントローラー設定
	float analogDeadzone = 0.2f;
	float analogRunThreshold = 0.7f;
	bool enableAnalogMovement = true;
	InputType inputType = InputType::Auto;

	// キャラクター回転設定
	float rotationSpeed = 8.0f;
	float rotationThreshold = 0.1f;
	bool enableSmoothRotate = true;
	bool enableCameraRelativMovement = true;
	bool rotateOnlyWhenMoving = true;

	// 攻撃ステップ設定
	bool attackStepEnabled = true;			// 攻撃時前進の有効・無効
	float attackStepMaxDistance = 1.5f;		// 攻撃ステップの最大移動距離（上限キャップ）
	float attackStepDuration = 0.12f;		// 攻撃ステップの補間時間（秒）

	// カメラ追従設定
	bool enableCameraFollow = true;                     // カメラ追従の有効化
	float cameraFollowSpeed = 5.0f;                     // カメラ追従の速度（緩やかに変更）
	float cameraRotationThreshold = 0.02f;              // カメラが動いていると判定する閾値（少し高めに）
	float cameraFollowDelay = 0.05f;                    // カメラが止まってからプレイヤー制御に戻るまでの遅延（短く）
};

// 入力状態
struct InputState {
	Vector3 moveDirection{ 0.0f, 0.0f, 0.0f };
	bool runPressed = false;

	float analogMagnitude = 0.0f;
	bool isAnalogInput = false;
	InputType currentInputType = InputType::Keyboard;
};