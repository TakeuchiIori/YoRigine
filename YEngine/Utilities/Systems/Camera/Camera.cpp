#include "Camera.h"
#include "WinApp/WinApp.h"
#include "MathFunc.h"


Camera::Camera()
    : transform_({ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} })
    , fovY_(0.45f)
    , aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight))
    , nearClip_(0.1f)
    , farClip_(1000.0f)
{
    InitializeCameraResource();
    UpdateMatrix();
}

void Camera::Update()
{

	cameraData_->worldPosition = transform_.translate;
	cameraData_->viewProjection = viewProjectionMatrix_;
    // シェイク中ならオフセットを計算
    if (cameraShake_.isShaking_) {
        UpdateShake();
    }
    else {
        shakeOffset_ = { 0,0,0 };
    }
}

void Camera::UpdateMatrix()
{
    // 1. Directorからもらった座標 + カメラ独自の演出（シェイク）を合成
    Vector3 finalPos = transform_.translate + shakeOffset_;

    // 2. アフィン変換・逆行列計算
    worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, finalPos);
    viewMatrix_ = Inverse(worldMatrix_);

    // 3. 射影行列計算
    projectionMatrix_ = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);

    // 4. 合成（これがレンダラーに渡される）
    viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);
}

void Camera::Shake(float time, const Vector2 min, const Vector2 max)
{
    cameraShake_.isShaking_ = true;
    cameraShake_.shakeTimer_ = 0.0f;
    cameraShake_.shakeDuration_ = time;
    cameraShake_.shakeMinRange_ = min;
    cameraShake_.shakeMaxRange_ = max;
}

void Camera::InitializeCameraResource()
{
    cameraResource_ =
       YoRigine::DirectXCommon::GetInstance()->CreateBufferResource(sizeof(CameraForGPU));

    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
    cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
}

void Camera::UpdateShake()
{
    float deltaTime = 1.0f / 60.0f;
    cameraShake_.shakeTimer_ += deltaTime;

    if (cameraShake_.shakeTimer_ >= cameraShake_.shakeDuration_) {
        cameraShake_.isShaking_ = false;
        shakeOffset_ = { 0,0,0 };
    }
    else {
        // ランダムな揺れを生成
        float rndX = static_cast<float>(rand()) / RAND_MAX;
        float rndY = static_cast<float>(rand()) / RAND_MAX;

        shakeOffset_.x = cameraShake_.shakeMinRange_.x + rndX * (cameraShake_.shakeMaxRange_.x - cameraShake_.shakeMinRange_.x);
        shakeOffset_.y = cameraShake_.shakeMinRange_.y + rndY * (cameraShake_.shakeMaxRange_.y - cameraShake_.shakeMinRange_.y);
    }
}