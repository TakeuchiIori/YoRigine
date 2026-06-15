#pragma once
#include "Vector3.h"
#include "MathFunc.h"
#include "Matrix4x4.h"
#include "Quaternion.h"

#include <Loaders/Json/Use/AutoJson.h>

class Line;

/// <summary>
/// 各コンポーネントの基底クラス
/// </summary>
class VirtualCamera
{
public:
	///************************* 基本関数 *************************///
	virtual ~VirtualCamera() = default;
	virtual void Initialize();
	virtual void Update() = 0;
	virtual void UpdateMatrix();
	virtual void DrawDebugGui() {}
	// シーン空間に Line でデバッグ描画する用（パス可視化など）
	virtual void DrawDebug3D(Line& /*line*/) {}
	virtual void Save(nlohmann::json& j) const;
	virtual void Load(const nlohmann::json& j);

	// 基底クラス用のJsonの登録
	virtual void RegisterJsonFields();
public:
	///************************* アクセッサ *************************///

	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	void SetFovY(float fovY) { fovY_ = fovY; }
	void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
	void SetNearClip(float nearClip) { nearClip_ = nearClip; }
	void SetFarClip(float farClip) { farClip_ = farClip; }

	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

	Vector3 GetRotate() const { return transform_.rotate; }
	Vector3 GetTranslate() const { return transform_.translate; }
	Vector3 GetScale() const { return transform_.scale; }

	float GetFovY() const { return fovY_; }
	float GetAspectRatio() const { return aspectRatio_; }
	float GetNearClip() const { return nearClip_; }
	float GetFarClip() const { return farClip_; }

	int GetPriority() const { return priority_; }

	void SetName(const std::string& name) { name_ = name; }
	const std::string& GetName() const { return name_; }
	void SetPriority(int priority) { priority_ = priority; }
public:
	///************************* メンバ変数 *************************///

	EulerTransform transform_;
	Matrix4x4 worldMatrix_;
	Matrix4x4 viewMatrix_;
	Matrix4x4 projectionMatrix_;
	Matrix4x4 viewProjectionMatrix_;
	AutoJson aj_;

	float fovY_ = 0.45f;
	float aspectRatio_;
	float nearClip_;
	float farClip_;
	// 優先度が高いカメラが自動的に選ばれる
	int priority_ = 10;

	std::string name_;
};