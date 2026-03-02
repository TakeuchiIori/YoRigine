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

/// <summary>
/// コンストラクタ：プレイヤーの移動コンポーネントを初期化
/// </summary>
/// <param name="owner">このコンポーネントを持つプレイヤー</param>
PlayerMovement::PlayerMovement(Player* owner) : owner_(owner) {
	InitializeStateMachine();
}

/// <summary>
/// ステートマシンの初期化
/// Idle, Moving などの状態を登録し、初期状態を設定する
/// </summary>
void PlayerMovement::InitializeStateMachine() {
	//---------------------------------------------------------------------------------------------
	// 各状態を登録
	//---------------------------------------------------------------------------------------------
	stateMachine_.RegisterState<IdleMovementState>(MovementState::Idle, this);
	stateMachine_.RegisterState<MovingState>(MovementState::Moving, this);

	//---------------------------------------------------------------------------------------------
	// 初期状態を設定
	//---------------------------------------------------------------------------------------------
	stateMachine_.SetInitialState(MovementState::Idle);

	// オーナーを設定
	stateMachine_.SetOwner(this);
}

/// <summary>
/// 毎フレーム更新処理
/// 入力、カメラ追従、ステートマシン更新を行う
/// </summary>
/// <param name="deltaTime">経過時間</param>
void PlayerMovement::Update(float deltaTime) {
	//---------------------------------------------------------------------------------------------
	// 入力切り替えクールダウンの処理
	//---------------------------------------------------------------------------------------------
	if (inputSwitchCooldown_ > 0.0f) {
		inputSwitchCooldown_ -= deltaTime;
	}

	//---------------------------------------------------------------------------------------------
	// カメラ追従処理（プレイヤーの向きをカメラに合わせる）
	//---------------------------------------------------------------------------------------------
	UpdateCameraFollow(deltaTime);

	//---------------------------------------------------------------------------------------------
	// ステートマシン更新（Idle/Movingなどの状態を更新）
	//---------------------------------------------------------------------------------------------
	stateMachine_.Update(deltaTime);

	//-----------------------------------------------------------------------------------------
	// 攻撃ステップ補間（ヒット時の前進を毎フレーム進行）
	//-----------------------------------------------------------------------------------------
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


/// <summary>
/// カメラ追従処理
/// プレイヤーの回転をカメラ方向に合わせる
/// </summary>
/// <param name="deltaTime">経過時間</param>
void PlayerMovement::UpdateCameraFollow(float deltaTime) {
	if (!config_.enableCameraFollow) {
		isCameraMoving_ = false;
		return;
	}

	// 現在のカメラ回転を取得
	Vector3 cameraRotation = GetCameraRotation();
	float currentCameraY = cameraRotation.y;

	//---------------------------------------------------------------------------------------------
	// カメラの回転変化量を算出し、動いているか判定
	//---------------------------------------------------------------------------------------------
	float cameraRotationDelta = std::abs(currentCameraY - previousCameraRotationY_);

	// 差分を -π ～ π に正規化
	while (cameraRotationDelta > std::numbers::pi_v<float>) {
		cameraRotationDelta -= 2.0f * std::numbers::pi_v<float>;
	}
	cameraRotationDelta = std::abs(cameraRotationDelta);

	//---------------------------------------------------------------------------------------------
	// カメラが動いている場合の処理
	//---------------------------------------------------------------------------------------------
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
	//---------------------------------------------------------------------------------------------
	// カメラが止まっている場合の処理
	//---------------------------------------------------------------------------------------------
	else {
		cameraStopTimer_ += deltaTime;

		// 一定時間経過したらカメラ追従を解除
		if (cameraStopTimer_ >= config_.cameraFollowDelay) {
			isCameraMoving_ = false;
		}
	}

	// 前フレームの回転値を保持
	previousCameraRotationY_ = currentCameraY;
}

/// <summary>
/// YoRigine::JsonManagerで移動関連の設定を登録
/// ImGuiや設定ファイルから変更できるようにする
/// </summary>
void PlayerMovement::InitJson(YoRigine::JsonManager* jsonManager) {
	//---------------------------------------------------------------------------------------------
	// 基本移動設定
	//---------------------------------------------------------------------------------------------
	jsonManager->SetTreePrefix("移動設定");
	jsonManager->Register("歩行速度", &config_.walkSpeed);
	jsonManager->Register("走行速度", &config_.runSpeed);
	jsonManager->Register("減速率", &config_.deceleration);
	jsonManager->Register("ダッシュ有効", &config_.enableDash);
	jsonManager->Register("走行有効", &config_.enableRun);

	//---------------------------------------------------------------------------------------------
	// 入力設定
	//---------------------------------------------------------------------------------------------
	jsonManager->SetTreePrefix("コントローラー設定");
	jsonManager->Register("デッドゾーン", &config_.analogDeadzone);
	jsonManager->Register("走行閾値", &config_.analogRunThreshold);
	jsonManager->Register("アナログ移動有効", &config_.enableAnalogMovement);

	//---------------------------------------------------------------------------------------------
	// 回転設定
	//---------------------------------------------------------------------------------------------
	jsonManager->SetTreePrefix("回転設定");
	jsonManager->Register("回転速度", &config_.rotationSpeed);
	jsonManager->Register("回転閾値", &config_.rotationThreshold);
	jsonManager->Register("滑らか回転", &config_.enableSmoothRotate);
	jsonManager->Register("カメラ基準移動", &config_.enableCameraRelativMovement);
	jsonManager->Register("移動中のみ回転", &config_.rotateOnlyWhenMoving);

	//---------------------------------------------------------------------------------------------

	//-----------------------------------------------------------------------------------------
	// 攻撃ステップ設定
	//-----------------------------------------------------------------------------------------
	jsonManager->SetTreePrefix("攻撃ステップ設定");
	jsonManager->Register("攻撃ステップ有効", &config_.attackStepEnabled);
	jsonManager->Register("攻撃ステップ最大距離", &config_.attackStepMaxDistance);
	jsonManager->Register("攻撃ステップ補間時間", &config_.attackStepDuration);
	// カメラ追従設定
	//---------------------------------------------------------------------------------------------
	jsonManager->SetTreePrefix("カメラ追従設定");
	jsonManager->Register("カメラ追従有効", &config_.enableCameraFollow);
	jsonManager->Register("カメラ追従速度", &config_.cameraFollowSpeed);
	jsonManager->Register("カメラ回転判定閾値", &config_.cameraRotationThreshold);
	jsonManager->Register("カメラ停止遅延", &config_.cameraFollowDelay);
}

/// <summary>
/// ステート遷移可能かを判定
/// 例：移動不可時はIdleへの遷移のみ許可
/// </summary>
bool PlayerMovement::CanTransitionTo([[maybe_unused]] MovementState newState) const {
	// 移動禁止中の場合はIdle以外に遷移不可
	if (!canMove_ && newState != MovementState::Idle) {
		return false;
	}
	return true;
}

/// <summary>
/// 現在の入力状態を取得
/// キーボード／コントローラーのどちらかを優先して使用
/// </summary>
InputState PlayerMovement::GetInputState() const {
	YoRigine::Input* input = YoRigine::Input::GetInstance();
	InputState state;

	//---------------------------------------------------------------------------------------------
	// コントローラー入力があるか確認
	//---------------------------------------------------------------------------------------------
	if (input->IsControllerConnected()) {
		state = GetControllerInput();

		// 入力があればアナログ入力として扱う
		if (state.moveDirection.Length() > 0.01f) {
			state.isAnalogInput = true;
			state.currentInputType = InputType::Gamepad;
			return state;
		}
	}

	//---------------------------------------------------------------------------------------------
	// キーボード入力をチェック
	//---------------------------------------------------------------------------------------------
	state = GetKeyboardInput();
	if (state.moveDirection.Length() > 0.01f) {
		state.isAnalogInput = false;
		state.currentInputType = InputType::Keyboard;
		return state;
	}

	//---------------------------------------------------------------------------------------------
	// 入力がない場合は前回の入力タイプを維持
	//---------------------------------------------------------------------------------------------
	state.currentInputType = lastInputType_;
	return state;
}

/// <summary>
/// キーボード入力の取得処理
/// </summary>
InputState PlayerMovement::GetKeyboardInput() const {
	YoRigine::Input* input = YoRigine::Input::GetInstance();
	InputState state;

	// WASD入力を取得
	if (input->PushKey(DIK_W)) state.moveDirection.z += 1.0f;
	if (input->PushKey(DIK_S)) state.moveDirection.z -= 1.0f;
	if (input->PushKey(DIK_A)) state.moveDirection.x -= 1.0f;
	if (input->PushKey(DIK_D)) state.moveDirection.x += 1.0f;

	// 入力方向を正規化
	if (state.moveDirection.Length() > 0.0f) {
		state.moveDirection = state.moveDirection.Normalize();
	}

	// Shift押下で走行扱いにする
	state.runPressed = input->PushKey(DIK_LSHIFT);

	return state;
}

/// <summary>
/// コントローラー入力の取得処理
/// 左スティック入力を処理して移動方向を算出
/// </summary>
InputState PlayerMovement::GetControllerInput() const {
	YoRigine::Input* input = YoRigine::Input::GetInstance();
	InputState state;

	//---------------------------------------------------------------------------------------------
	// 左スティック入力を取得
	//---------------------------------------------------------------------------------------------
	float lx = input->GetLeftStickX(0);
	float ly = input->GetLeftStickY(0);

	//---------------------------------------------------------------------------------------------
	// デッドゾーンを適用（小さい入力を無視）
	//---------------------------------------------------------------------------------------------
	lx = ApplyDeadzone(lx, config_.analogDeadzone);
	ly = ApplyDeadzone(ly, config_.analogDeadzone);

	// 入力方向を設定
	state.moveDirection.x = lx;
	state.moveDirection.z = ly;

	// 入力の強さを算出（0.0～1.0）
	state.analogMagnitude = std::sqrt(lx * lx + ly * ly);
	if (state.analogMagnitude > 1.0f) {
		state.analogMagnitude = 1.0f;
	}

	// 一定以上の倒し具合で走行扱い
	state.runPressed = state.analogMagnitude >= config_.analogRunThreshold;

	return state;
}


/// <summary>
/// 入力デバイス（キーボード or コントローラー）の切り替え検出
/// </summary>
void PlayerMovement::DetectInputType(const InputState& input) {
	// クールダウン中は切り替え無効
	if (inputSwitchCooldown_ > 0.0f) return;

	// 入力タイプが変化した場合のみイベント発火
	if (input.currentInputType != lastInputType_) {
		lastInputType_ = input.currentInputType;
		inputSwitchCooldown_ = 0.5f;

		// コールバック通知（UIなどで利用可能）
		if (onInputTypeChanged_) {
			onInputTypeChanged_(input.currentInputType);
		}
	}
}

/// <summary>
/// プレイヤーの回転処理
/// 移動方向またはカメラ方向にスムーズに向きを合わせる
/// </summary>
void PlayerMovement::UpdateRotate(float deltaTime, const Vector3& moveDirection) {
	if (!canRotate_) return; // 回転禁止なら無視

	// 十分な入力がない場合は回転しない
	if (moveDirection.Length() < config_.rotationThreshold) return;

	// ターゲット方向を計算
	targetRotateY_ = CalculateTargetRotate(moveDirection);

	//---------------------------------------------------------------------------------------------
	// スムーズ回転または即時回転
	//---------------------------------------------------------------------------------------------
	if (config_.enableSmoothRotate) {
		float t = config_.rotationSpeed * deltaTime;
		currentRotateY_ = LerpAngle(currentRotateY_, targetRotateY_, t);
		isRotating_ = std::abs(currentRotateY_ - targetRotateY_) > 0.01f;
	} else {
		currentRotateY_ = targetRotateY_;
		isRotating_ = false;
	}
}

/// <summary>
/// 入力方向ベクトルからY軸の回転角を計算
/// </summary>
float PlayerMovement::CalculateTargetRotate(const Vector3& direction) const {
	return std::atan2(direction.x, direction.z);
}

/// <summary>
/// 角度補間処理（ラジアンベース）
/// 角度のラップアラウンド（-π～π）を考慮した線形補間
/// </summary>
float PlayerMovement::LerpAngle(float from, float to, float t) const {
	float diff = to - from;

	// 角度差を -π～π の範囲に収める
	while (diff > std::numbers::pi_v<float>) diff -= 2.0f * std::numbers::pi_v<float>;
	while (diff < -std::numbers::pi_v<float>) diff += 2.0f * std::numbers::pi_v<float>;

	// 線形補間
	return from + diff * t;
}


/// <summary>
/// デッドゾーンを適用（スティックの微小入力を無視）
/// </summary>
float PlayerMovement::ApplyDeadzone(float value, float deadzone) const {
	if (std::abs(value) < deadzone) return 0.0f;
	float sign = (value > 0.0f) ? 1.0f : -1.0f;
	return sign * ((std::abs(value) - deadzone) / (1.0f - deadzone));
}

/// <summary>
/// 移動の適用処理
/// 現在の速度ベクトルをポジションに加算
/// </summary>
void PlayerMovement::ApplyMovement(float deltaTime) {
	if (owner_) {
		// 現在位置を取得
		Vector3 pos = owner_->GetWorldPosition();

		// 移動速度を時間でスケーリングして加算
		pos += velocity_ * deltaTime;

		// 新しい位置を設定
		owner_->SetPosition(pos);
	}
}

/// <summary>
/// 攻撃ステップを要求する（補間で敵の方向へ前進）
/// ヒット時に敵の位置を受け取り、その方向にstepDistanceだけ補間で移動する
/// </summary>
/// <param name="targetPosition">敵の世界座標</param>
/// <param name="stepDistance">前進距離（attackData.stepDistance）</param>
void PlayerMovement::RequestAttackStep(const Vector3& targetPosition, float stepDistance) {
	if (!owner_ || !config_.attackStepEnabled) return;
	if (stepDistance <= 0.0f) return;

	// 上限キャップ適用
	float clampedDistance = std::min(stepDistance, config_.attackStepMaxDistance);

	// プレイヤー現在位置から敵への方向を算出（Y軸は無視）
	Vector3 currentPos = owner_->GetWorldPosition();
	Vector3 toEnemy = targetPosition - currentPos;
	toEnemy.y = 0.0f;
	float dist = toEnemy.Length();

	// 敵と重なっていたら前進しない
	if (dist < 0.01f) return;

	// 方向を正規化してstepDistanceだけの目標位置を計算
	Vector3 direction = toEnemy * (1.0f / dist);
	Vector3 target = currentPos + direction * clampedDistance;

	// 補間データをセットして開始
	stepStartPos_ = currentPos;
	stepTargetPos_ = target;
	stepProgress_ = 0.0f;
	isAttackStepping_ = true;
}

/// <summary>
/// 回転の適用処理
/// 現在のY回転値をワールドトランスフォームに反映
/// </summary>
void PlayerMovement::ApplyRotate() {
	if (owner_) {
		auto& wt = owner_->GetWT();
		wt.rotate_.y = currentRotateY_;
	}
}

/// <summary>
/// カメラ基準での移動方向を計算
/// （プレイヤー入力方向をカメラの向きに合わせて変換）
/// 例：Wで前進＝カメラの前方向へ
/// </summary>
Vector3 PlayerMovement::CameraMoveDir(const Vector3& inputDirection, const Vector3& cameraRotation) {
	// カメラの回転行列を作成
	Matrix4x4 cameraRotateMatrix = MakeRotateMatrixXYZ(cameraRotation);

	// カメラの前方向を取得
	Vector3 cameraForward = Normalize({
		cameraRotateMatrix.m[2][0],
		0.0f,
		cameraRotateMatrix.m[2][2]
		});

	// カメラの右方向を取得
	Vector3 cameraRight = Normalize({
		cameraRotateMatrix.m[0][0],
		0.0f,
		cameraRotateMatrix.m[0][2]
		});

	// 入力方向をカメラ基準に変換
	Vector3 moveDir = {
		cameraForward.x * inputDirection.z + cameraRight.x * inputDirection.x,
		0.0f,
		cameraForward.z * inputDirection.z + cameraRight.z * inputDirection.x
	};

	return moveDir;
}

/// <summary>
/// 現在のカメラ回転を取得
/// </summary>
Vector3 PlayerMovement::GetCameraRotation() const {
	if (owner_) {
		return owner_->GetCameraRotation();
	}
	return Vector3(0.0f, 0.0f, 0.0f);
}

/// <summary>
/// プレイヤーの正面方向を取得
/// </summary>
Vector3 PlayerMovement::GetForwardDirection() const {
	float y = currentRotateY_;
	return Vector3(std::sin(y), 0.0f, std::cos(y));
}

/// <summary>
/// 現在の移動速度を取得
/// </summary>
float PlayerMovement::GetSpeed() const {
	return velocity_.Length();
}

/// <summary>
/// 移動中かどうかを判定
/// </summary>
bool PlayerMovement::IsMoving() const {
	return velocity_.Length() > 0.01f;
}

/// <summary>
/// 即座に移動を停止
/// 速度と移動方向をリセットする
/// </summary>
void PlayerMovement::ForceStop() {
	velocity_ = Vector3(0.0f, 0.0f, 0.0f);
	targetDirection_ = Vector3(0.0f, 0.0f, 0.0f);
}

/// <summary>
/// MovementState を文字列に変換
/// </summary>
const char* PlayerMovement::GetStateString(MovementState state) const {
	switch (state) {
	case MovementState::Idle: return "Idle";
	case MovementState::Moving: return "Moving";
	case MovementState::Jump:  return "Jump";
	case MovementState::Stunned: return "Stunned";
	default: return "Unknown";
	}
}

/// <summary>
/// デバッグ情報（ImGui）
/// 現在の移動状態、速度、カメラ追従状況などを可視化
/// </summary>
void PlayerMovement::ShowStateDebug() {
#ifdef USE_IMGUI
	// タブバーの開始
	if (ImGui::BeginTabBar("移動状態"))
	{
		// --- タブ2: 移動・回転 ---
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

		// --- タブ3: 制御権 ---
		if (ImGui::BeginTabItem("操作設定"))
		{
			ImGui::Text("移動可能: %s", canMove_ ? "可能" : "禁止");
			ImGui::Text("回転可能: %s", canRotate_ ? "可能" : "禁止");
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar(); // 全てのタブを終了
	}
#endif
}