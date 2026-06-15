#include "VirtualCamera.h"
#include "WinApp/WinApp.h"

void VirtualCamera::Initialize() {
    transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    aspectRatio_ = float(WinApp::kClientWidth) / float(WinApp::kClientHeight);
    nearClip_ = 0.1f;
    farClip_ = 1000.0f;
    fovY_ = 0.45f;
    RegisterJsonFields();
}

void VirtualCamera::UpdateMatrix() {
    // 1. ワールド行列の計算
    worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

    // 2. ビュー行列の計算 (カメラのワールドの逆行列)
    viewMatrix_ = Inverse(worldMatrix_);

    // 3. 射影行列の計算
    projectionMatrix_ = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);

    // 4. 合成
    viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);
}

void VirtualCamera::Save(nlohmann::json& j) const {
    aj_.Save(j);
}

void VirtualCamera::Load(const nlohmann::json& j) {
    aj_.Load(j);
}

// 基底クラス用のJson登録
void VirtualCamera::RegisterJsonFields()
{
    aj_.Add("name", &name_);
    aj_.Add("priority", &priority_);
    aj_.Add("translate", &transform_.translate);
    aj_.Add("rotate", &transform_.rotate);
    aj_.Add("fovY", &fovY_);
}
