#include "PlayerMovement.h"
#include "../Player.h"
#include "Generators/Trigger/WaypointManager.h"

// State
#include "IdleMovementState.h"
#include "MovingState.h"

// Engine
#include "Systems/Input./Input.h"
#include "MathFunc.h"

#include <algorithm>
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
	// デッドゾーンの調整元は MovementConfig 側に置いたままにする。
	// エディタで値を変えたその場で効くよう、毎フレーム同期する。
	// ------------------------------------------------------------
	if (owner_) {
		if (PlayerInput* input = owner_->GetPlayerInput()) {
			input->SetMoveDeadzone(config_.analogDeadzone);
		}
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
	// フィールド誘導: 現在のウェイポイント方向へ向きを補正
	// ------------------------------------------------------------
	UpdateWaypointFacing(deltaTime);

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

	// ------------------------------------------------------------
	// オートホーミング（位置 + Yaw の補間）
	// ------------------------------------------------------------
	if (isHoming_ && owner_) {
		homingTimer_ += deltaTime;
		float t = (homingDuration_ > 0.0f) ? std::min(homingTimer_ / homingDuration_, 1.0f) : 1.0f;
		// SmoothStep
		float s = t * t * (3.0f - 2.0f * t);

		// 位置補間
		Vector3 pos = homingStartPos_ + (homingTargetPos_ - homingStartPos_) * s;
		owner_->SetPosition(pos);

		// Yaw補間（最短経路で）
		float diff = homingTargetYaw_ - homingStartYaw_;
		while (diff <= -std::numbers::pi_v<float>) diff += 2.0f * std::numbers::pi_v<float>;
		while (diff >   std::numbers::pi_v<float>) diff -= 2.0f * std::numbers::pi_v<float>;
		currentRotateY_ = homingStartYaw_ + diff * s;
		targetRotateY_ = currentRotateY_;
		ApplyRotate();

		if (t >= 1.0f) {
			isHoming_ = false;
			homingTimer_ = 0.0f;
			// 突進終了で無敵解除
			owner_->SetInvincible(false);
		}
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
// ウェイポイント方向への向き補正
// ============================================================
void PlayerMovement::UpdateWaypointFacing(float deltaTime) {
	if (!owner_ || !config_.enableWaypointFacing) return;
	if (!canRotate_ || isHoming_) return;
	if (owner_->IsBattleMode()) return;
	if (config_.waypointFaceOnlyWhenIdle && IsMoving()) return;

	auto* waypoint = WaypointManager::GetInstance();
	if (!waypoint || !waypoint->HasCurrent()) return;

	Vector3 toWaypoint = waypoint->GetCurrentPosition() - owner_->GetWorldPosition();
	toWaypoint.y = 0.0f;

	const float distance = toWaypoint.Length();
	if (distance <= config_.waypointFacingMinDistance) return;

	Vector3 direction = toWaypoint * (1.0f / distance);
	targetRotateY_ = CalculateTargetRotate(direction);

	const float t = std::clamp(config_.waypointFacingSpeed * deltaTime, 0.0f, 1.0f);
	currentRotateY_ = LerpAngle(currentRotateY_, targetRotateY_, t);
	isRotating_ = std::abs(currentRotateY_ - targetRotateY_) > 0.01f;
	ApplyRotate();
}

// ============================================================
// 現在のウェイポイント方向へ即時向き合わせ
// ============================================================
bool PlayerMovement::FaceCurrentWaypointNow() {
	if (!owner_) return false;
	if (!canRotate_ || isHoming_) return false;
	if (owner_->IsBattleMode()) return false;

	auto* waypoint = WaypointManager::GetInstance();
	if (!waypoint || !waypoint->HasCurrent()) return false;

	Vector3 toWaypoint = waypoint->GetCurrentPosition() - owner_->GetWorldPosition();
	toWaypoint.y = 0.0f;

	const float distance = toWaypoint.Length();
	if (distance <= config_.waypointFacingMinDistance) return false;

	Vector3 direction = toWaypoint * (1.0f / distance);
	targetRotateY_ = CalculateTargetRotate(direction);
	currentRotateY_ = targetRotateY_;
	isRotating_ = false;
	ApplyRotate();
	return true;
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

	// ------------------------------------------------------------
	// ウェイポイント誘導設定
	// ------------------------------------------------------------
	jsonManager->SetTreePrefix("ウェイポイント誘導設定");
	jsonManager->Register("ウェイポイント方向を向く", &config_.enableWaypointFacing);
	jsonManager->Register("停止中のみ向く", &config_.waypointFaceOnlyWhenIdle);
	jsonManager->Register("向く速度", &config_.waypointFacingSpeed);
	jsonManager->Register("最小距離", &config_.waypointFacingMinDistance);
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
	InputState state;

	PlayerInput* input = owner_ ? owner_->GetPlayerInput() : nullptr;
	if (!input) {
		state.currentInputType = lastInputType_;
		return state;
	}

	// 移動軸はスティックとWASDの両方を束ねた1本の値として受け取る。
	// どちらのデバイス由来かは axis.isAnalog / axis.device が持っている。
	const YoRigine::InputAxisValue axis = input->MoveAxis();
	state.moveDirection.x = axis.value.x;
	state.moveDirection.z = axis.value.y;
	state.isAnalogInput = axis.isAnalog;

	// 走りはデバイスで判定が変わる。
	// スティックは倒し量、キーボードは Run アクション（既定 LSHIFT）。
	state.runPressed = axis.isAnalog
		? (axis.magnitude >= config_.analogRunThreshold)
		: input->RunHeld();

	if (axis.magnitude > 0.01f) {
		state.currentInputType = axis.isAnalog ? InputType::Gamepad : InputType::Keyboard;
		// analogMagnitude はスティック入力のときだけ意味を持たせる（従来の挙動を維持）。
		state.analogMagnitude = axis.isAnalog ? axis.magnitude : 0.0f;
		return state;
	}

	// ------------------------------------------------------------
	// 入力がない場合は前回の入力タイプを維持
	// ------------------------------------------------------------
	state.currentInputType = lastInputType_;
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
// オートホーミングの要求
// 攻撃発動時に呼ばれ、指定ターゲット方向へ位置と向きを補正する
// ============================================================
void PlayerMovement::RequestAutoHoming(const Vector3& targetPosition, float duration, float maxStep) {
	if (!owner_ || duration <= 0.0f) return;

	Vector3 currentPos = owner_->GetWorldPosition();
	Vector3 toEnemy = targetPosition - currentPos;
	toEnemy.y = 0.0f;
	float dist = toEnemy.Length();
	if (dist < 0.01f) return;

	Vector3 direction = toEnemy * (1.0f / dist);

	// 前進距離は maxStep 以内、かつターゲットを通り過ぎないように制限
	float stepLen = std::min(maxStep, dist);

	homingStartPos_ = currentPos;
	homingTargetPos_ = currentPos + direction * stepLen;

	homingStartYaw_ = currentRotateY_;
	homingTargetYaw_ = std::atan2f(direction.x, direction.z);

	homingDuration_ = duration;
	homingTimer_ = 0.0f;
	isHoming_ = true;

	// 突進中は敵の攻撃で潰されない（i-frame）。踏み込みを必ず出し切らせる。
	owner_->SetInvincible(true);
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
// 戦闘アクション終了時のアニメーション再同期
// 現在のMovementステートと入力状態に基づき、適切なアニメーションを再生する
// ============================================================
void PlayerMovement::SyncAnimationToCurrentState() {
	if (!owner_) return;

	auto* obj = owner_->GetObject3d();
	if (!obj) return;

	MovementState currentState = GetCurrentState();

	if (currentState == MovementState::Moving) {
		// 移動中：入力状態から歩行/走行を判定してアニメーション再生
		InputState input = GetInputState();
		bool isRunning = input.runPressed && config_.enableRun;

		obj->SetMotionSpeed(owner_->GetMotionSpeed(0));

		if (isRunning) {
			obj->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Run1");
		}
		else {
			obj->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Walk1");
		}
	}
	else {
		// 待機中：Idleアニメーションを再生
		obj->SetMotionSpeed(owner_->GetMotionSpeed(0));
		obj->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Idle4");
	}
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
