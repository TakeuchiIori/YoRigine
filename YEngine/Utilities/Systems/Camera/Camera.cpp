#include "Camera.h"
#include "WinApp/WinApp.h"
#include "MathFunc.h"

// ============================================================
// コンストラクタ
// ============================================================
Camera::Camera()
	: transform_({ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} })
	, fovY_(0.45f)
	, aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight))
	, nearClip_(0.1f)
	, farClip_(1000.0f)
{
	// ------------------------------------------------------------
	// GPU用リソースの初期化と行列の初期計算
	// ------------------------------------------------------------
	InitializeCameraResource();
	UpdateMatrix();
}

// ============================================================
// 毎フレームの更新処理
// ============================================================
void Camera::Update()
{
	// ------------------------------------------------------------
	// GPUデータの更新
	// ------------------------------------------------------------
	cameraData_->worldPosition = transform_.translate;
	cameraData_->viewProjection = viewProjectionMatrix_;

	// ------------------------------------------------------------
	// シェイク中のオフセット計算
	// ------------------------------------------------------------
	if (cameraShake_.isShaking_) {
		UpdateShake();
	}
	else {
		shakeOffset_ = { 0,0,0 };
	}
}

// ============================================================
// 行列の再計算
// ============================================================
void Camera::UpdateMatrix()
{
	// ------------------------------------------------------------
	// Directorからもらった座標と独自の演出（シェイク）を合成
	// ------------------------------------------------------------
	Vector3 finalPos = transform_.translate + shakeOffset_;

	// ------------------------------------------------------------
	// アフィン変換と逆行列計算（ビュー行列）
	// ------------------------------------------------------------
	worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, finalPos);
	viewMatrix_ = Inverse(worldMatrix_);

	// ------------------------------------------------------------
	// 射影行列計算
	// ------------------------------------------------------------
	projectionMatrix_ = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);

	// ------------------------------------------------------------
	// ビュープロジェクション行列の合成
	// ------------------------------------------------------------
	viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);
}

// ============================================================
// 画面揺らしの開始
// ============================================================
void Camera::Shake(float time, const Vector2 min, const Vector2 max)
{
	cameraShake_.isShaking_ = true;
	cameraShake_.shakeTimer_ = 0.0f;
	cameraShake_.shakeDuration_ = time;
	cameraShake_.shakeMinRange_ = min;
	cameraShake_.shakeMaxRange_ = max;
}

// ============================================================
// カメラリソースの初期化
// ============================================================
void Camera::InitializeCameraResource()
{
	cameraResource_ = YoRigine::DirectXCommon::GetInstance()->CreateBufferResource(sizeof(CameraForGPU));

	cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
	cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
}

// ============================================================
// シェイク更新処理
// ============================================================
void Camera::UpdateShake()
{
	// ------------------------------------------------------------
	// タイマーの進行と終了判定
	// ------------------------------------------------------------
	float deltaTime = 1.0f / 60.0f;
	cameraShake_.shakeTimer_ += deltaTime;

	if (cameraShake_.shakeTimer_ >= cameraShake_.shakeDuration_) {
		cameraShake_.isShaking_ = false;
		shakeOffset_ = { 0,0,0 };
	}
	else {
		// ------------------------------------------------------------
		// ランダムな揺れ幅の生成
		// ------------------------------------------------------------
		float rndX = static_cast<float>(rand()) / RAND_MAX;
		float rndY = static_cast<float>(rand()) / RAND_MAX;

		shakeOffset_.x = cameraShake_.shakeMinRange_.x + rndX * (cameraShake_.shakeMaxRange_.x - cameraShake_.shakeMinRange_.x);
		shakeOffset_.y = cameraShake_.shakeMinRange_.y + rndY * (cameraShake_.shakeMaxRange_.y - cameraShake_.shakeMinRange_.y);
	}
}