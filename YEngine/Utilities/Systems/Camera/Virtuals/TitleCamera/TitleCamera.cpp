#include "TitleCamera.h"
#include <Systems/GameTime/GameTime.h>

void TitleCamera::Initialize()
{
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
}

void TitleCamera::Save(nlohmann::json& j) const
{
}

void TitleCamera::Load(const nlohmann::json& j)
{
}
