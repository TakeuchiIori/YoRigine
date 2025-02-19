#include "TopDownCamera.h"
#include "MathFunc.h"
#include "Matrix4x4.h"
#include <DirectXMath.h>

#ifdef _DEBUG
#include <imgui.h>
#endif // _DEBUG

void TopDownCamera::Initialize()
{
}

void TopDownCamera::Update()
{
	TopDownProsess();
#ifdef _DEBUG
	ImGui();
#endif // _DEBUG

}

void TopDownCamera::TopDownProsess()
{
	// ターゲットがない場合は処理しない
	if (target_ == nullptr)
	{
		return;
	}

	rotate_ = { DirectX::XMConvertToRadians(90), 0.0f, 0.0f };

	translate_ = target_->translation_ + offset_;
	
	matView_ = Inverse(MakeAffineMatrix(scale_, rotate_, translate_));
}

void TopDownCamera::ImGui()
{
	//ImGui::Begin("TopDownCamera Info");
	//ImGui::DragFloat3("OffSet", &offset_.x);

	//ImGui::DragFloat3("Translate", &translate_.x);

	//ImGui::DragFloat3("Rotation", &rotate_.x);

	//ImGui::DragFloat3("Scale", &scale_.x);

	//ImGui::End();
}


