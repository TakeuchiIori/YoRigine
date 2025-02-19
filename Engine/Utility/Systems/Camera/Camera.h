#pragma once
// Math
#include "Vector3.h"
#include "MathFunc.h"
#include "Matrix4x4.h"

class Camera
{
public: // メンバ関数

	struct CameraShake {
		float shakeTimer_ = 0.0f;
		float shakeDuration_ = 0.0f;
		Vector2 shakeMinRange_;
		Vector2 shakeMaxRange_;
		Vector3 originalPosition_;
		bool isShaking_ = false;
	};
	/// <summary>
	/// コンストラクタ
	/// </summary>
	Camera();

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 行列の更新
	/// </summary>
	void UpdateMatrix();


	/// <summary>
	/// ImGui
	/// </summary>
	void ShowImGui();

public: // カメラ

	/// <summary>
    /// カメラの位置を原点にリセットする
    /// </summary>
	void DefaultCamera();


public: // シェイク

	void Shake(float time, const Vector2 min, const Vector2 max);

private:

	void UpdateShake();

public: // アクセッサ
    // Setter
    void SetRotate(const Vector3& rotate) {transform_.rotate = rotate;}
    void SetTranslate(const Vector3& translate) {transform_.translate = translate; }
	void SetFovY(float fovY) { fovY_ = fovY; }
    void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio;}
	void SetNearClip(float nearClip) { nearClip_ = nearClip; }
    void SetFarClip(float farClip) {farClip_ = farClip;     }

	// getter
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
	Vector3 GetRotate() const { return transform_.rotate; }
	Vector3 GetTranslate() const { return transform_.translate; }
	Vector3 GetScale() const { return transform_.scale; }

public: 
	EulerTransform transform_;	   
	Matrix4x4 worldMatrix_;	   
	Matrix4x4 viewMatrix_;	   
	Matrix4x4 projectionMatrix_;  // 投影行列
	Matrix4x4 viewProjectionMatrix_;
	float fovY_;        // 水平方向視野角 (Field of View)
	float aspectRatio_;          // アスペクト比
	float nearClip_;			 // ニアクリップ距離
	float farClip_;				 // ファークリップ距離

	/*===============================================================//
								シェイク
	//===============================================================*/

	CameraShake cameraShake_;
	Vector3 shakeOffset_;
};

