#include "DebugCamera.h"
#include "MathFunc.h"
#include <Systems/Input/Input.h>
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void DebugCamera::Initialize() {
	VirtualCamera::Initialize(); // 基底の初期化（Scaleなどを1に）

	// 初期位置の設定
	transform_.translate = { 0.0f, 6.0f, -40.0f };

	YoRigine::Input* input = YoRigine::Input::GetInstance();
	prevMousePos_ = input->GetMousePosition();
}

void DebugCamera::Update() {
	if (YoRigine::Input::GetInstance()->PushKey(DIK_LCONTROL)&& 
		YoRigine::Input::GetInstance()->PushKey(DIK_LSHIFT)) {
		isMoving_ = !isMoving_;
	}

	if (!isMoving_) return;
	UpdateInput();
}

void DebugCamera::UpdateInput() {
	YoRigine::Input* input = YoRigine::Input::GetInstance();
	Vector2 currentMousePos = input->GetMousePosition();

	//------------------------------------------------------------
	// マウスによる回転 (右ドラッグ)
	//------------------------------------------------------------
	if (input->IsPressMouse(1)) {
		if (!isDragging_) {
			isDragging_ = true;
			prevMousePos_ = currentMousePos;
		}
		float deltaX = currentMousePos.x - prevMousePos_.x;
		float deltaY = currentMousePos.y - prevMousePos_.y;

		// 基底の rotate を更新
		transform_.rotate.x += deltaY * rotateSpeed_ * 0.1f;
		transform_.rotate.y += deltaX * rotateSpeed_ * 0.1f;
		prevMousePos_ = currentMousePos;
	}
	else {
		isDragging_ = false;
	}

	// 現在の回転から行列を作成（移動方向の計算用）
	Matrix4x4 rotMat = MakeRotateMatrixXYZ(transform_.rotate);

	//------------------------------------------------------------
	// キーボードによる移動 (WASD)
	//------------------------------------------------------------
	Vector3 move{ 0, 0, 0 };
	if (input->PushKey(DIK_W)) move.z += moveSpeed_;
	if (input->PushKey(DIK_S)) move.z -= moveSpeed_;
	if (input->PushKey(DIK_A)) move.x -= moveSpeed_;
	if (input->PushKey(DIK_D)) move.x += moveSpeed_;

	if (input->PushKey(DIK_Q)) move.y -= moveSpeed_;
	if (input->PushKey(DIK_E)) move.y += moveSpeed_;


	// 回転に合わせて移動方向を変換して加算
	transform_.translate += TransformNormal(move, rotMat);

	//------------------------------------------------------------
	// コントローラーによる操作
	//------------------------------------------------------------
	if (input->IsControllerConnected()) {
		XINPUT_STATE joyState;
		if (input->GetJoystickState(0, joyState)) {
			// 右スティックで回転
			transform_.rotate.x += static_cast<float>(joyState.Gamepad.sThumbRY) * rotateSpeedController_;
			transform_.rotate.y += static_cast<float>(joyState.Gamepad.sThumbRX) * rotateSpeedController_;

			// 左スティックで移動 (デッドゾーン 8000)
			Vector3 stickMove{ 0, 0, 0 };
			if (std::abs(joyState.Gamepad.sThumbLX) > 8000) stickMove.x = static_cast<float>(joyState.Gamepad.sThumbLX);
			if (std::abs(joyState.Gamepad.sThumbLY) > 8000) stickMove.z = static_cast<float>(joyState.Gamepad.sThumbLY);

			if (Length(stickMove) > 0) {
				stickMove = Normalize(stickMove) * moveSpeedController_;
				transform_.translate += TransformNormal(stickMove, rotMat);
			}

			// L/Rトリガーで垂直移動
			if (joyState.Gamepad.bLeftTrigger > 0) {
				transform_.translate.y -= moveSpeedController_ * 0.1f * (joyState.Gamepad.bLeftTrigger / 255.0f);
			}
			if (joyState.Gamepad.bRightTrigger > 0) {
				transform_.translate.y += moveSpeedController_ * 0.1f * (joyState.Gamepad.bRightTrigger / 255.0f);
			}
		}
	}
}

void DebugCamera::DrawDebugGui() {
#ifdef USE_IMGUI
	ImGui::Text("デバッグカメラ設定");
	ImGui::Checkbox("有効", &isMoving_);
	ImGui::DragFloat("移動独度（キーボード）", &moveSpeed_, 0.1f);
	ImGui::DragFloat("回転速度（キーボード）", &rotateSpeed_, 0.01f);

	ImGui::Separator();
	ImGui::Text("パッド設定");
	ImGui::DragFloat("移動速度 (パッド)", &moveSpeedController_, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("回転速度 (パッド)", &rotateSpeedController_, 0.001f, 0.0f, 0.5f);

	ImGui::Separator();
	ImGui::DragFloat3("位置", &transform_.translate.x, 0.1f);
	ImGui::DragFloat3("回転", &transform_.rotate.x, 0.01f);

	float fov = GetFovY();
	if (ImGui::SliderAngle("FOV", &fov, 10.0f, 120.0f)) {
		SetFovY(fov);
	}
#endif
}

void DebugCamera::Save(nlohmann::json& j) const {
	VirtualCamera::Save(j); // 名前や座標などの共通項目
	j["moveSpeed"] = moveSpeed_;
	j["rotateSpeed"] = rotateSpeed_;
	j["moveSpeedController"] = moveSpeedController_;
	j["rotateSpeedController"] = rotateSpeedController_;
}

void DebugCamera::Load(const nlohmann::json& j) {
	VirtualCamera::Load(j);
	moveSpeed_ = j.value("moveSpeed", 0.5f);
	rotateSpeed_ = j.value("rotateSpeed", 0.05f);
	moveSpeedController_ = j.value("moveSpeedController", 0.1f);
	rotateSpeedController_ = j.value("rotateSpeedController", 0.005f);
}