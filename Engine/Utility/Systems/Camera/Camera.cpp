#include "Camera.h"
// C++
#include <cmath>
// Engine
#include "WinApp./WinApp.h"

// Math
#include "MathFunc.h"
#include "Matrix4x4.h"
#include "Systems/Input/Input.h"

#ifdef _DEBUG
#include "imgui.h"
#endif // _DEBUG

Camera::Camera()
    : transform_({ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} })
    , fovY_(0.45f)
    , aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight))
    , nearClip_(0.1f)
    , farClip_(100.0f)
    , worldMatrix_(MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate))
    , viewMatrix_(Inverse(worldMatrix_))
    , projectionMatrix_(MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_))
    , viewProjectionMatrix_(Multiply(viewMatrix_, projectionMatrix_))
{}

void Camera::Update()
{
	if (cameraShake_.isShaking_) {
		UpdateShake();
	}
}

void Camera::UpdateMatrix()
{
    // transformからアフィン変換行列を計算
    worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate + shakeOffset_);
    // worldMatrixの逆行列
    viewMatrix_ = Inverse(worldMatrix_);
    // プロジェクション行列の更新
    projectionMatrix_ = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
    // 合成行列
    viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);
}

void Camera::ShowImGui()
{
#ifdef _DEBUG
    ImGui::Begin("Camera ");
    ImGui::DragFloat3("Camera Position", &transform_.translate.x, 0.01f);
    ImGui::DragFloat3("Camera Rotate", &transform_.rotate.x, 0.01f);
    ImGui::End();
#endif
}

void Camera::DefaultCamera()
{
    // カメラの位置と回転をリセット（原点に設定）
    transform_.translate = Vector3(0.0f, 0.0f, -10.0f);
    transform_.rotate = Vector3(0.0f, 0.0f, 0.0f);

    // ワールド行列、ビュー行列の更新
    worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    viewMatrix_ = Inverse(worldMatrix_);
    viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);

}

void Camera::Shake(float time, const Vector2 min, const Vector2 max)
{
     cameraShake_.isShaking_ = true;
     cameraShake_.shakeTimer_ = 0.0f;
     cameraShake_.shakeDuration_ = time;
     cameraShake_.shakeMinRange_ = min;
     cameraShake_.shakeMaxRange_ = max;
     cameraShake_.originalPosition_ = transform_.translate;
}

void Camera::UpdateShake()
{
    if (cameraShake_.isShaking_) {
		float deltaTime = 1.0f / 60.0f;
        cameraShake_.shakeTimer_ += deltaTime;

        if (cameraShake_.shakeTimer_ >= cameraShake_.shakeDuration_) {
            // シェイク終了
            cameraShake_.isShaking_ = false;
            transform_.translate = cameraShake_.originalPosition_;
        }
        else {
            // シェイクの強さを計算（X方向とY方向で別々に）
            float progressRatio = cameraShake_.shakeTimer_ / cameraShake_.shakeDuration_;
            Vector2 shakePower = {
                std::lerp(cameraShake_.shakeMaxRange_.x, cameraShake_.shakeMinRange_.x, progressRatio),
                std::lerp(cameraShake_.shakeMaxRange_.y, cameraShake_.shakeMinRange_.y, progressRatio)
            };

            // ランダムなシェイクオフセットを計算
            shakeOffset_ = {
                (float)(rand() % 20 - 10) / 10.0f * shakePower.x,
                (float)(rand() % 20 - 10) / 10.0f * shakePower.y,
                0.0f
            };
        }
    }
}
