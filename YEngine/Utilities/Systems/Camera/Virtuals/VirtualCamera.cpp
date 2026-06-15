#include "VirtualCamera.h"
#include "WinApp/WinApp.h"

void VirtualCamera::Initialize() {
    transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    aspectRatio_ = float(WinApp::kClientWidth) / float(WinApp::kClientHeight);
    nearClip_ = 0.1f;
    farClip_ = 1000.0f;
    fovY_ = 0.45f;
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
    // 共通パラメータの保存
    j["name"] = name_;
    j["priority"] = priority_;
    j["translate"] = { transform_.translate.x, transform_.translate.y, transform_.translate.z };
    j["rotate"] = { transform_.rotate.x, transform_.rotate.y, transform_.rotate.z };
    j["fovY"] = fovY_;
}

void VirtualCamera::Load(const nlohmann::json& j) {
    // 共通パラメータの読み込み
    name_ = j["name"];
    priority_ = j["priority"];
    transform_.translate = { j["translate"][0], j["translate"][1], j["translate"][2] };
    transform_.rotate = { j["rotate"][0], j["rotate"][1], j["rotate"][2] };
    fovY_ = j["fovY"];
}