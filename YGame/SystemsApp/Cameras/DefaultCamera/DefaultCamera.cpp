#include "DefaultCamera.h"
#include "MathFunc.h"
#include "Matrix4x4.h"
#include <Systems/Input/Input.h>
#include <DirectXMath.h>
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include <Systems/GameTime/GameTime.h>

/// <summary>
/// 初期化処理
/// </summary>
void DefaultCamera::Initialize()
{
	// カメラ初期位置設定
	translate_ = { 0.0f, 6.0f, -40.0f };

	// 初期マウス位置を取得
	YoRigine::Input* input = YoRigine::Input::GetInstance();
	prevMousePos_ = input->GetMousePosition();

	InitJson();
}

/// <summary>
/// 更新処理
/// </summary>
void DefaultCamera::Update()
{
	if (enableOrbit_ && target_) {
		//------------------------------------------------------------
		// オービットモード（ターゲット中心に回転）
		//------------------------------------------------------------
		orbitAngle_ += orbitSpeed_ * YoRigine::GameTime::GetDeltaTime();
		if (orbitAngle_ > 2.0f * std::numbers::pi_v<float>) {
			orbitAngle_ -= 2.0f * std::numbers::pi_v<float>;
		}

		Vector3 targetPos = target_->translate_;
		translate_.x = targetPos.x + cosf(orbitAngle_) * orbitRadius_;
		translate_.z = targetPos.z + sinf(orbitAngle_) * orbitRadius_;
		translate_.y = targetPos.y + orbitHeight_;

		Vector3 forward = Normalize(targetPos - translate_);
		rotate_.y = atan2f(forward.x, forward.z);
		rotate_.x = asinf(forward.y);

		matView_ = Inverse(MakeAffineMatrix(scale_, rotate_, translate_));
	} else {
#ifdef USE_IMGUI
		//------------------------------------------------------------
		// 通常（自由移動）モード
		//------------------------------------------------------------
		UpdateInput();
		matView_ = Inverse(MakeAffineMatrix(scale_, rotate_, translate_));
#endif
	}
}

/// <summary>
/// 入力処理（マウス・キーボード・コントローラー）
/// </summary>
void DefaultCamera::UpdateInput()
{
	YoRigine::Input* input = YoRigine::Input::GetInstance();

	//------------------------------------------------------------
	// マウス操作
	//------------------------------------------------------------
	Vector2 currentMousePos = input->GetMousePosition();

	// 右クリックでカメラ回転
	if (input->IsPressMouse(1)) {
		if (!isDragging_) {
			isDragging_ = true;
			prevMousePos_ = currentMousePos;
		}

		float deltaX = currentMousePos.x - prevMousePos_.x;
		float deltaY = currentMousePos.y - prevMousePos_.y;

		rotate_.y += deltaX * rotateSpeed_ * 0.01f;
		rotate_.x += deltaY * rotateSpeed_ * 0.01f;

		rotate_.x = std::clamp(rotate_.x, -1.5f, 1.5f);
	} else {
		isDragging_ = false;
	}

	prevMousePos_ = currentMousePos;

	//------------------------------------------------------------
	// キーボード操作（WASDQE）
	//------------------------------------------------------------
	Vector3 moveDirection = { 0, 0, 0 };

	if (input->PushKey(DIK_W)) moveDirection.z += 1.0f;
	if (input->PushKey(DIK_S)) moveDirection.z -= 1.0f;
	if (input->PushKey(DIK_A)) moveDirection.x -= 1.0f;
	if (input->PushKey(DIK_D)) moveDirection.x += 1.0f;
	if (input->PushKey(DIK_Q) || input->IsPressMouse(3)) moveDirection.y += 1.0f;
	if (input->PushKey(DIK_E) || input->IsPressMouse(4)) moveDirection.y -= 1.0f;

	float speedMultiplier = (input->PushKey(DIK_LSHIFT) || input->PushKey(DIK_RSHIFT)) ? 3.0f : 1.0f;

	if (Length(moveDirection) > 0.0f) {
		moveDirection = Normalize(moveDirection);

		Matrix4x4 rotMat = MakeRotateMatrixXYZ(rotate_);
		Vector3 transformedMove = TransformNormal(moveDirection, rotMat);

		translate_ += transformedMove * moveSpeed_ * speedMultiplier;
	}

	//------------------------------------------------------------
	// コントローラー操作
	//------------------------------------------------------------
	if (input->IsControllerConnected()) {
		XINPUT_STATE joyState;
		if (input->GetJoystickState(0, joyState)) {

			// 右スティックで回転
			Vector3 rotateMove{};
			rotateMove.y += static_cast<float>(joyState.Gamepad.sThumbRX) * rotateSpeedController_ * 0.0001f;
			rotateMove.x -= static_cast<float>(joyState.Gamepad.sThumbRY) * rotateSpeedController_ * 0.0001f;

			rotate_ += rotateMove;
			rotate_.x = std::clamp(rotate_.x, -1.5f, 1.5f);

			// 左スティックで移動
			Vector3 stickMove{};
			stickMove.x = static_cast<float>(joyState.Gamepad.sThumbLX);
			stickMove.z = static_cast<float>(joyState.Gamepad.sThumbLY);

			if (Length(stickMove) > 0.0f) {
				stickMove = Normalize(stickMove) * moveSpeedController_ * 0.1f;

				Matrix4x4 rotMat = MakeRotateMatrixXYZ(rotate_);
				Vector3 transformedMove = TransformNormal(stickMove, rotMat);

				translate_ += transformedMove;
			}

			// トリガーで上下移動
			if (joyState.Gamepad.bLeftTrigger > 0) {
				translate_.y -= moveSpeedController_ * 0.1f * (joyState.Gamepad.bLeftTrigger / 255.0f);
			}
			if (joyState.Gamepad.bRightTrigger > 0) {
				translate_.y += moveSpeedController_ * 0.1f * (joyState.Gamepad.bRightTrigger / 255.0f);
			}
		}
	}
}

/// <summary>
/// JSON初期化（パラメータ登録）
/// </summary>
void DefaultCamera::InitJson()
{
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("DefaultCamera", "Resources/Json/Cameras");
	jsonManager_->SetCategory("Cameras");
	jsonManager_->SetSubCategory("DefaultCamera");

	jsonManager_->Register("Translate", &translate_);
	jsonManager_->Register("Rotate", &rotate_);
	jsonManager_->Register("RotateSpeed", &rotateSpeed_);
	jsonManager_->Register("RotateSpeed Controller", &rotateSpeedController_);
	jsonManager_->Register("MoveSpeed", &moveSpeed_);
	jsonManager_->Register("MoveSpeed Controller", &moveSpeedController_);
}

/// <summary>
/// デバッグ用ImGui表示
/// </summary>
void DefaultCamera::ImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("DefaultCamera Info");

	ImGui::DragFloat3("Position", &translate_.x, 0.1f);
	ImGui::DragFloat3("Rotation", &rotate_.x, 0.01f);
	ImGui::DragFloat("Rotate Speed", &rotateSpeed_, 0.01f, 0.01f, 2.0f);
	ImGui::DragFloat("Move Speed", &moveSpeed_, 0.1f, 0.1f, 10.0f);

	ImGui::End();
#endif
}
