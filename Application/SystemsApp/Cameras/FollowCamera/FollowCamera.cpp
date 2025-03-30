#include "FollowCamera.h"
#include "MathFunc.h"
#include "Matrix4x4.h"
#include <Systems/Input/Input.h>
#include <DirectXMath.h>

#ifdef _DEBUG
#include <imgui.h>
#endif // _DEBUG

void FollowCamera::Initialize()
{
	InitJson();
}

void FollowCamera::Update()
{

	UpdateInput();

	FollowProsess();
	
	
	//jsonManager_->ImGui("FollowCamera");
}

void FollowCamera::UpdateInput()
{
	if (Input::GetInstance()->IsControllerConnected())
	{

		XINPUT_STATE joyState;
		if (Input::GetInstance()->GetJoystickState(0, joyState)) {
			const float kRotateSpeed = 0.07f;

			Vector3 move{};
			move.x = 0;
			move.y += static_cast<float>(joyState.Gamepad.sThumbRX);
			move.z = 0;

			// 移動量に速さを反映
			if (Length(move) > 0.0f) {
				move = Normalize(move) * kRotateSpeed;
			}
			else {
				move = { 0.0f, 0.0f, 0.0f };
			}

			rotate_ += move;
		}
	}
}

void FollowCamera::FollowProsess()
{
	// ターゲットがない場合は処理しない
	if (target_ == nullptr)
	{
		return;
	}
	Vector3 offset = offset_;

	

	Matrix4x4 rotate = MakeRotateMatrixXYZ(rotate_);

	offset = TransformNormal(offset, rotate);

	translate_ = target_->translation_ + offset;

	matView_ = Inverse(MakeAffineMatrix(scale_, rotate_, translate_));
}

void FollowCamera::InitJson()
{
	jsonManager_ = std::make_unique<JsonManager>("FollowCamera", "Resources/Json/Cameras");
	jsonManager_->SetCategory("Cameras");
	jsonManager_->Register("OffSet Translate", &offset_);
	jsonManager_->Register("Rotate", &rotate_);
}

void FollowCamera::JsonImGui()
{
	
}

void FollowCamera::ImGui()
{
	//ImGui::Begin("FollowCamera Info");

	//ImGui::DragFloat3("Translate", &translate_.x);

	//ImGui::DragFloat3("Rotation", &rotate_.x);

	//ImGui::DragFloat3("Scale", &scale_.x);

	//ImGui::End();
}


