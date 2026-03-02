#pragma once
#include "Vector3.h"
#include "MathFunc.h"
#include "Matrix4x4.h"
#include <wrl.h>
#include "DirectXCommon.h"

/// <summary>
/// 出力用カメラクラス
/// Directorからの計算結果を受け取り、描画用の最終行列を生成する
/// </summary>
class Camera
{
public:
    struct CameraShake {
        float shakeTimer_ = 0.0f;
        float shakeDuration_ = 0.0f;
        Vector2 shakeMinRange_;
        Vector2 shakeMaxRange_;
        bool isShaking_ = false;
    };

    // カメラ
    struct CameraForGPU {
        Vector3 worldPosition;
        float padding;
        Matrix4x4 viewProjection;
    };

public:
    Camera();

    // 毎フレームの更新（シェイクのタイマー進捗など）
    void Update();

    // 行列の再計算（Directorから値をセットした後に呼ぶ）
    void UpdateMatrix();

    // 画面揺らしの開始
    void Shake(float time, const Vector2 min, const Vector2 max);

public:
    ///************************* アクセッサ（Directorから流し込む用） *************************///

    void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
    void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
    void SetFovY(float fovY) { fovY_ = fovY; }
    void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
    void SetNearClip(float nearClip) { nearClip_ = nearClip; }
    void SetFarClip(float farClip) { farClip_ = farClip; }

    const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
    const Vector3& GetTranslate() const { return transform_.translate; }

	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetCameraResource() const { return cameraResource_; }
	CameraForGPU* GetCameraData() const { return cameraData_; }
private:
	void InitializeCameraResource();
    void UpdateShake();

public:
    // メンバ変数は既存の描画システムが参照するため公開
    EulerTransform transform_;
    Matrix4x4 worldMatrix_;
    Matrix4x4 viewMatrix_;
    Matrix4x4 projectionMatrix_;
    Matrix4x4 viewProjectionMatrix_;

    float fovY_;
    float aspectRatio_;
    float nearClip_;
    float farClip_;

    // 演出用
    Vector3 shakeOffset_ = { 0,0,0 };
    CameraShake cameraShake_;

	// GPU転送用
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
    CameraForGPU* cameraData_ = nullptr;
};