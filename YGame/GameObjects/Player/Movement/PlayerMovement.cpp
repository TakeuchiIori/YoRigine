#include "PlayerMovement.h"
#include "../Player.h"

// State
#include "IdleMovementState.h"
#include "MovingState.h"

// Engine
#include "Systems/Input./Input.h"
#include "MathFunc.h"

#include <numbers>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

// ============================================================
// コンストラクタ
// ============================================================
PlayerMovement::PlayerMovement(Player* owner) : owner_(owner) {
	InitializeStateMachine();
}

// ============================================================
// ステートマシンの初期化
// ============================================================
void PlayerMovement::InitializeStateMachine() {
	// ------------------------------------------------------------
	// 各状態を登録
	// ------------------------------------------------------------
	stateMachine_.RegisterState<IdleMovementState>(MovementState::Idle, this);
	stateMachine_.RegisterState<MovingState>(MovementState::Moving, this);

	// ------------------------------------------------------------
	// 初期状態を設定
	// ------------------------------------------------------------
	stateMachine_.SetInitialState(MovementState::Idle);
	stateMachine_.SetOwner(this);
}

// ============================================================
// 毎フレーム更新処理
// ============================================================
void PlayerMovement::Update(float deltaTime) {
	// ------------------------------------------------------------
	// 入力切り替えクールダウンの処理
	// ------------------------------------------------------------
	if (inputSwitchCooldown_ > 0.0f) {
		inputSwitchCooldown_ -= deltaTime;
	}

	// ------------------------------------------------------------
	// カメラ追従処理（プレイヤーの向きをカメラに合わせる）
	// ------------------------------------------------------------
	UpdateCameraFollow(deltaTime);

	// ------------------------------------------------------------
	// ステートマシン更新（Idle/Movingなどの状態を更新）
	// ------------------------------------------------------------
	stateMachine_.Update(deltaTime);

	// ------------------------------------------------------------
	// 攻撃ステップ補間（ヒット時の前進を毎フレーム進行）
	// ------------------------------------------------------------
	if (isAttackStepping_ && owner_) {
		stepProgress_ += deltaTime / config_.attackStepDuration;

		if (stepProgress_ >= 1.0f) {
			stepProgress_ = 1.0f;
			isAttackStepping_ = false;
		}

		// SmoothStep（三次補間）で自然な減速を実現
		float t = stepProgress_ * stepProgress_ * (3.0f - 2.0f * stepProgress_);
		Vector3 pos = stepStartPos_ + (stepTargetPos_ - stepStartPos_) * t;
		owner_->SetPosition(pos);
	}
}

// ============================================================
// カメラ追従処理
// ============================================================
void PlayerMovement::UpdateCameraFollow(float deltaTime) {
	if (!config_.enableCameraFollow) {
		isCameraMoving_ = false;
		return;
	}

	Vector3 cameraRotation = GetCameraRotation();
	float currentCameraY = cameraRotation.y;

	// ------------------------------------------------------------
	// カメラの回転変化量を算出し、動いているか判定
	// ------------------------------------------------------------
	float cameraRotationDelta = std::abs(currentCameraY - previousCameraRotationY_);

	// 差分を -π ～ π に正規化
	while (cameraRotationDelta > std::numbers::pi_v<float>) {
		cameraRotationDelta -= 2.0f * std::numbers::pi_v<float>;
	}
	cameraRotationDelta = std::abs(cameraRotationDelta);

	// ------------------------------------------------------------
	// カメラが動いている場合の処理
	// ------------------------------------------------------------
	if (cameraRotationDelta > config_.cameraRotationThreshold) {
		isCameraMoving_ = true;
		cameraStopTimer_ = 0.0f;

		// 移動中でない場合のみカメラ方向にプレイヤーを追従させる
		if (canRotate_ && !IsMoving()) {
			float t = config_.cameraFollowSpeed * deltaTime;
			currentRotateY_ = LerpAngle(currentRotateY_, currentCameraY, t);
			targetRotateY_ = currentCameraY;
		}
	}
	// ------------------------------------------------------------
	// カメラが止まっている場合の処理
	// ------------------------------------------------------------
	else {
		cameraStopTimer_ += deltaTime;

		if (cameraStopTimer_ >= config_.cameraFollowDelay) {
			isCameraMoving_ = false;
		}
	}

	previousCameraRotationY_ = currentCameraY;
}

// ============================================================
// JSON設定の初期化
// ============================================================
void PlayerMovement::InitJson(YoRigine::JsonManager* jsonManager) {
	// ------------------------------------------------------------
	// 基本移動設定
	// ------------------------------------------------------------
	jsonManager->SetTreePrefix("移動設定");
	jsonManager->Register("歩行速度", &config_.walkSpeed);
	jsonManager->Register("走行速度", &config_.runSpeed);
	jsonManager->Register("減速率", &config_.deceleration);
	jsonManager->Register("ダッシュ有効", &config_.enableDash);
	jsonManager->Register("走行有効", &config_.enableRun);

	// ------------------------------------------------------------
	// 入力設定
	// ------------------------------------------------------------
	jsonManager->SetTreePrefix("コントローラー設定");
	jsonManager->Register("デッドゾーン", &config_.analogDeadzone);
	jsonManager->Register("走行閾値", &config_.analogRunThreshold);
	jsonManager->Register("アナログ移動有効", &config_.enableAnalogMovement);

	// ------------------------------------------------------------
	// 回転設定
	// ------------------------------------------------------------
	jsonManager->SetTreePrefix("回転設定");
	jsonManager->Register("回転速度", &config_.rotationSpeed);
	jsonManager->Register("回転閾値", &config_.rotationThreshold);
	jsonManager->Register("滑らか回転", &config_.enableSmoothRotate);
	jsonManager->Register("カメラ基準移動", &config_.enableCameraRelativMovement);
	jsonManager->Register("移動中のみ回転", &config_.rotateOnlyWhenMoving);

	// ------------------------------------------------------------
	// 攻撃ステップ設定
	// ------------------------------------------------------------
	jsonManager->SetTreePrefix("攻撃ステップ設定");
	jsonManager->Register("攻撃ステップ有効", &config_.attackStepEnabled);
	jsonManager->Register("攻撃ステップ最大距離", &config_.attackStepMaxDistance);
	jsonManager->Register("攻撃ステップ補間時間", &config_.attackStepDuration);

	// ------------------------------------------------------------
	// カメラ追従設定
	// ------------------------------------------------------------
	jsonManager->SetTreePrefix("カメラ追従設定");
	jsonManager->Register("カメラ追従有効", &config_.enableCameraFollow);
	jsonManager->Register("カメラ追従速度", &config_.cameraFollowSpeed);
	jsonManager->Register("カメラ回転判定閾値", &config_.cameraRotationThreshold);
	jsonManager->Register("カメラ停止遅延", &config_.cameraFollowDelay);
}

// ============================================================
// ステート遷移可能性の判定
// ============================================================
bool PlayerMovement::CanTransitionTo([[maybe_unused]] MovementState newState) const {
	if (!canMove_ && newState != MovementState::Idle) {
		return false;
	}
	return true;
}

// ============================================================
// 入力状態の取得
// ============================================================
InputState PlayerMovement::GetInputState() const {
	YoRigine::Input* input = YoRigine::Input::GetInstance();
	InputState state;

	// ------------------------------------------------------------
	// コントローラー入力を確認
	// ------------------------------------------------------------
	if (input->IsControllerConnected()) {
		state = GetControllerInput();

		if (state.moveDirection.Length() > 0.01f) {
			state.isAnalogInput = true;
			state.currentInputType = InputType::Gamepad;
			return state;
		}
	}

	// ------------------------------------------------------------
	// キーボード入力を確認
	// ------------------------------------------------------------
	state = GetKeyboardInput();
	if (state.moveDirection.Length() > 0.01f) {
		state.isAnalogInput = false;
		state.currentInputType = InputType::Keyboard;
		return state;
	}

	// ------------------------------------------------------------
	// 入力がない場合は前回の入力タイプを維持
	// ------------------------------------------------------------
	state.currentInputType = lastInputType_;
	return state;
}

// ============================================================
// キーボード入力取得
// ============================================================
InputState PlayerMovement::GetKeyboardInput() const {
	YoRigine::Input* input = YoRigine::Input::GetInstance();
	InputState state;

	if (input->PushKey(DIK_W)) state.moveDirection.z += 1.0f;
	if (input->PushKey(DIK_S)) state.moveDirection.z -= 1.0f;
	if (input->PushKey(DIK_A)) state.moveDirection.x -= 1.0f;
	if (input->PushKey(DIK_D)) state.moveDirection.x += 1.0f;

	if (state.moveDirection.Length() > 0.0f) {
		state.moveDirection = state.moveDirection.Normalize();
	}

	state.runPressed = input->PushKey(DIK_LSHIFT);

	return state;
}

// ============================================================
// コントローラー入力取得
// ============================================================
InputState PlayerMovement::GetControllerInput() const {
	YoRigine::Input* input = YoRigine::Input::GetInstance();
	InputState state;

	// ------------------------------------------------------------
	// 左スティック入力を取得
	// ------------------------------------------------------------
	float lx = input->GetLeftStickX(0);
	float ly = input->GetLeftStickY(0);

	// ------------------------------------------------------------
	// デッドゾーンを適用
	// ------------------------------------------------------------
	lx = ApplyDeadzone(lx, config_.analogDeadzone);
	ly = ApplyDeadzone(ly, config_.analogDeadzone);

	state.moveDirection.x = lx;
	state.moveDirection.z = ly;

	state.analogMagnitude = std::sqrt(lx * lx + ly * ly);
	if (state.analogMagnitude > 1.0f) {
		state.analogMagnitude = 1.0f;
	}

	state.runPressed = state.analogMagnitude >= config_.analogRunThreshold;

	return state;
}

// ============================================================
// 入力デバイスの切り替え検出
// ============================================================
void PlayerMovement::DetectInputType(const InputState& input) {
	if (inputSwitchCooldown_ > 0.0f) return;

	if (input.currentInputType != lastInputType_) {
		lastInputType_ = input.currentInputType;
		inputSwitchCooldown_ = 0.5f;

		if (onInputTypeChanged_) {
			onInputTypeChanged_(input.currentInputType);
		}
	}
}

// ============================================================
// プレイヤーの回転処理
// ============================================================
void PlayerMovement::UpdateRotate(float deltaTime, const Vector3& moveDirection) {
	if (!canRotate_) return;

	if (moveDirection.Length() < config_.rotationThreshold) return;

	targetRotateY_ = CalculateTargetRotate(moveDirection);

	// ------------------------------------------------------------
	// スムーズ回転または即時回転
	// ------------------------------------------------------------
	if (config_.enableSmoothRotate) {
		float t = config_.rotationSpeed * deltaTime;
		currentRotateY_ = LerpAngle(currentRotateY_, targetRotateY_, t);
		isRotating_ = std::abs(currentRotateY_ - targetRotateY_) > 0.01f;
	}
	else {
		currentRotateY_ = targetRotateY_;
		isRotating_ = false;
	}
}

// ============================================================
// 目標回転角の計算
// ============================================================
float PlayerMovement::CalculateTargetRotate(const Vector3& direction) const {
	return std::atan2(direction.x, direction.z);
}

// ============================================================
// 角度の線形補間
// ============================================================
float PlayerMovement::LerpAngle(float from, float to, float t) const {
	float diff = to - from;

	while (diff > std::numbers::pi_v<float>) diff -= 2.0f * std::numbers::pi_v<float>;
	while (diff < -std::numbers::pi_v<float>) diff += 2.0f * std::numbers::pi_v<float>;

	return from + diff * t;
}

// ============================================================
// デッドゾーン適用処理
// ============================================================
float PlayerMovement::ApplyDeadzone(float value, float deadzone) const {
	if (std::abs(value) < deadzone) return 0.0f;
	float sign = (value > 0.0f) ? 1.0f : -1.0f;
	return sign * ((std::abs(value) - deadzone) / (1.0f - deadzone));
}

// ============================================================
// 移動の適用
// ============================================================
void PlayerMovement::ApplyMovement(float deltaTime) {
	if (owner_) {
		Vector3 pos = owner_->GetWorldPosition();
		pos += velocity_ * deltaTime;
		owner_->SetPosition(pos);
	}
}

// ============================================================
// 攻撃ステップの要求
// ============================================================
void PlayerMovement::RequestAttackStep(const Vector3& targetPosition, float stepDistance) {
	if (!owner_ || !config_.attackStepEnabled) return;
	if (stepDistance <= 0.0f) return;

	float clampedDistance = std::min(stepDistance, config_.attackStepMaxDistance);

	Vector3 currentPos = owner_->GetWorldPosition();
	Vector3 toEnemy = targetPosition - currentPos;
	toEnemy.y = 0.0f;
	float dist = toEnemy.Length();

	if (dist < 0.01f) return;

	Vector3 direction = toEnemy * (1.0f / dist);
	Vector3 target = currentPos + direction * clampedDistance;

	stepStartPos_ = currentPos;
	stepTargetPos_ = target;
	stepProgress_ = 0.0f;
	isAttackStepping_ = true;
}

// ============================================================
// 回転の適用
// ============================================================
void PlayerMovement::ApplyRotate() {
	if (owner_) {
		auto& wt = owner_->GetWT();
		wt.rotate_.y = currentRotateY_;
	}
}

// ============================================================
// カメラ相対の移動方向計算
// ============================================================
Vector3 PlayerMovement::CameraMoveDir(const Vector3& inputDirection, const Vector3& cameraRotation) {
	Matrix4x4 cameraRotateMatrix = MakeRotateMatrixXYZ(cameraRotation);

	Vector3 cameraForward = Normalize({
		cameraRotateMatrix.m[2][0],
		0.0f,
		cameraRotateMatrix.m[2][2]
		});

	Vector3 cameraRight = Normalize({
		cameraRotateMatrix.m[0][0],
		0.0f,
		cameraRotateMatrix.m[0][2]
		});

	Vector3 moveDir = {
		cameraForward.x * inputDirection.z + cameraRight.x * inputDirection.x,
		0.0f,
		cameraForward.z * inputDirection.z + cameraRight.z * inputDirection.x
	};

	return moveDir;
}

// ============================================================
// カメラ回転取得
// ============================================================
Vector3 PlayerMovement::GetCameraRotation() const {
	if (owner_) {
		return owner_->GetCameraRotation();
	}
	return Vector3(0.0f, 0.0f, 0.0f);
}

// ============================================================
// 正面方向取得
// ============================================================
Vector3 PlayerMovement::GetForwardDirection() const {
	float y = currentRotateY_;
	return Vector3(std::sin(y), 0.0f, std::cos(y));
}

// ============================================================
// 速度取得
// ============================================================
float PlayerMovement::GetSpeed() const {
	return velocity_.Length();
}

// ============================================================
// 移動中判定
// ============================================================
bool PlayerMovement::IsMoving() const {
	return velocity_.Length() > 0.01f;
}

// ============================================================
// 即時停止
// ============================================================
void PlayerMovement::ForceStop() {
	velocity_ = Vector3(0.0f, 0.0f, 0.0f);
	targetDirection_ = Vector3(0.0f, 0.0f, 0.0f);
}

// ============================================================
// 状態名の文字列取得
// ============================================================
const char* PlayerMovement::GetStateString(MovementState state) const {
	switch (state) {
	case MovementState::Idle: return "Idle";
	case MovementState::Moving: return "Moving";
	case MovementState::Jump:  return "Jump";
	case MovementState::Stunned: return "Stunned";
	default: return "Unknown";
	}
}

// ============================================================
// デバッグ表示（ImGui）
// ============================================================
void PlayerMovement::ShowStateDebug() {
#ifdef USE_IMGUI
	if (ImGui::BeginTabBar("移動状態"))
	{
		// ------------------------------------------------------------
		// 移動・回転情報
		// ------------------------------------------------------------
		if (ImGui::BeginTabItem("移動・回転"))
		{
			ImGui::Text("=== 移動情報 ===");
			ImGui::Text("移動スピード: %.2f", GetSpeed());
			ImGui::Text("移動中フラグ: %s", IsMoving() ? "はい" : "いいえ");

			ImGui::Separator();

			ImGui::Text("=== 回転情報 ===");
			ImGui::Text("現在の角度(Y): %.2f", currentRotateY_);
			ImGui::Text("目標の角度(Y): %.2f", targetRotateY_);
			ImGui::Text("回転中フラグ: %s", isRotating_ ? "はい" : "いいえ");
			ImGui::EndTabItem();
		}

		// ------------------------------------------------------------
		// 操作設定フラグ情報
		// ------------------------------------------------------------
		if (ImGui::BeginTabItem("操作設定"))
		{
			ImGui::Text("移動可能: %s", canMove_ ? "可能" : "禁止");
			ImGui::Text("回転可能: %s", canRotate_ ? "可能" : "禁止");
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
#endif
}