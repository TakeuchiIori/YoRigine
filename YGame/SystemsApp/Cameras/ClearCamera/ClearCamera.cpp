#include "ClearCamera.h"
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
void ClearCamera::Initialize()
{
	translate_ = { 0.0f, 6.0f, -40.0f };

	YoRigine::Input* input = YoRigine::Input::GetInstance();
	prevMousePos_ = input->GetMousePosition();

	InitJson();
}

/// <summary>
/// 更新処理
/// </summary>
void ClearCamera::Update()
{
	if (enableOrbit_ && target_) {

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
		UpdateInput();
		matView_ = Inverse(MakeAffineMatrix(scale_, rotate_, translate_));
#endif
	}
}

/// <summary>
/// 入力処理
/// </summary>
void ClearCamera::UpdateInput()
{
	YoRigine::Input* input = YoRigine::Input::GetInstance();
	Vector2 currentMousePos = input->GetMousePosition();

	if (input->IsPressMouse(1)) {
		if (!isDragging_) {
			isDragging_ = true;
			prevMousePos_ = currentMousePos;
		}

		float dx = currentMousePos.x - prevMousePos_.x;
		float dy = currentMousePos.y - prevMousePos_.y;

		rotate_.y += dx * rotateSpeed_ * 0.01f;
		rotate_.x += dy * rotateSpeed_ * 0.01f;
		rotate_.x = std::clamp(rotate_.x, -1.5f, 1.5f);
	} else {
		isDragging_ = false;
	}

	prevMousePos_ = currentMousePos;

	Vector3 moveDir{};

	if (input->PushKey(DIK_W)) moveDir.z += 1.0f;
	if (input->PushKey(DIK_S)) moveDir.z -= 1.0f;
	if (input->PushKey(DIK_A)) moveDir.x -= 1.0f;
	if (input->PushKey(DIK_D)) moveDir.x += 1.0f;
	if (input->PushKey(DIK_Q) || input->IsPressMouse(3)) moveDir.y += 1.0f;
	if (input->PushKey(DIK_E) || input->IsPressMouse(4)) moveDir.y -= 1.0f;

	float multiplier = (input->PushKey(DIK_LSHIFT) || input->PushKey(DIK_RSHIFT)) ? 3.0f : 1.0f;

	if (Length(moveDir) > 0.0f) {
		moveDir = Normalize(moveDir);
		Matrix4x4 rotMat = MakeRotateMatrixXYZ(rotate_);
		Vector3 transformed = TransformNormal(moveDir, rotMat);
		translate_ += transformed * moveSpeed_ * multiplier;
	}
}

/// <summary>
/// JSON初期化
/// </summary>
void ClearCamera::InitJson()
{
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("ClearCamera", "Resources/Json/Cameras");
	jsonManager_->SetCategory("Cameras");
	jsonManager_->SetSubCategory("ClearCamera");

	jsonManager_->Register("Translate", &translate_);
	jsonManager_->Register("Rotate", &rotate_);
	jsonManager_->Register("RotateSpeed", &rotateSpeed_);
	jsonManager_->Register("RotateSpeed Controller", &rotateSpeedController_);
	jsonManager_->Register("MoveSpeed", &moveSpeed_);
	jsonManager_->Register("MoveSpeed Controller", &moveSpeedController_);
}
