#include "TitleCamera.h"
#include <Systems/GameTime/GameTime.h>
#include <Systems/Input/Input.h>

#ifdef USE_IMGUI
#include "imgui.h"
#endif // USE_IMGUI


void TitleCamera::Initialize()
{
	VirtualCamera::Initialize();
	// カメラ初期位置設定
	transform_.translate = { 0.0f, 6.0f, -40.0f };

	// 初期マウス位置を取得
	YoRigine::Input* input = YoRigine::Input::GetInstance();
	prevMousePos_ = input->GetMousePosition();

}

void TitleCamera::Update()
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
		 transform_.translate.x = targetPos.x + cosf(orbitAngle_) * orbitRadius_;
		 transform_.translate.z = targetPos.z + sinf(orbitAngle_) * orbitRadius_;
		 transform_.translate.y = targetPos.y + orbitHeight_;

		Vector3 forward = Normalize(targetPos -  transform_.translate);
		 transform_.rotate.y = atan2f(forward.x, forward.z);
		 transform_.rotate.x = asinf(forward.y);

		 viewMatrix_ = Inverse(MakeAffineMatrix(transform_.scale, transform_.rotate,transform_.translate));
	}
}

void TitleCamera::DrawDebugGui()
{
#ifdef USE_IMGUI
	ImGui::Text("カメラ設定");
	ImGui::DragFloat3("位置", &transform_.translate.x, 0.1f);
	ImGui::DragFloat3("回転", &transform_.rotate.x, 0.01f);
	ImGui::Separator();
	ImGui::DragFloat("回転速度", &orbitSpeed_, 0.01f);
	ImGui::DragFloat("回り込み半径", &orbitRadius_, 0.1f);
	ImGui::DragFloat("回り込み高さ", &orbitHeight_, 0.1f);
	ImGui::DragFloat("角度", &orbitAngle_, 0.1f);
	ImGui::Checkbox("オービットモード", &enableOrbit_);
#endif
}

void TitleCamera::Save(nlohmann::json& j) const
{
	VirtualCamera::Save(j); // 名前や座標などの共通項目
	j["orbitSpeed"] = orbitSpeed_;
	j["orbitRadius"] = orbitRadius_;
	j["orbitHeight"] = orbitHeight_;
	j["enableOrbit"] = enableOrbit_;
	j["orbitAngle"] = orbitAngle_;
}

void TitleCamera::Load(const nlohmann::json& j)
{
	VirtualCamera::Load(j);
	orbitSpeed_ = j.value("orbitSpeed", 0.3f);
	orbitRadius_ = j.value("orbitRadius", 25.0f);
	orbitHeight_ = j.value("orbitHeight", 4.0f);
	enableOrbit_ = j.value("enableOrbit", 0.005f);
	orbitAngle_ = j.value("orbitAngle", 0.0f);
}
