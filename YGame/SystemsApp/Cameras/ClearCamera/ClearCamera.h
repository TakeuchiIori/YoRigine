#pragma once
#include <Vector3.h>
#include <WorldTransform/WorldTransform.h>
#include "Loaders/Json/JsonManager.h"

/// <summary>
/// クリアシーン用カメラクラス
/// </summary>
class ClearCamera
{
public:
	void Initialize();
	void Update();
	void UpdateInput();

	Vector3 translate_ = { 0, 0, -40.0f };
	Vector3 scale_ = { 1, 1, 1 };
	Vector3 rotate_ = { 0, 0, 0 };

	Matrix4x4 matView_ = {};
	bool  enableOrbit_ = false;

	void SetTarget(const WorldTransform& target) { target_ = &target; }
	void SetPosition(const Vector3& position) { translate_ = position; }
	Vector3 GetPosition() const { return translate_; }
	float GetFov() const { return (fov_ >= 110.0f) ? fov_ : fov_; }

private:
	void ImGui();
	void InitJson();

private:
	std::unique_ptr<YoRigine::JsonManager> jsonManager_;
	const WorldTransform* target_;

	Vector2 prevMousePos_ = { 0.0f, 0.0f };
	bool isDragging_ = false;

	float rotateSpeed_ = 0.05f;
	float rotateSpeedController_ = 0.005f;
	float moveSpeed_ = 0.5f;
	float moveSpeedController_ = 0.1f;
	float fov_ = 0.90f;

	// 回り込みカメラ用パラメータ
	float orbitRadius_ = 25.0f;
	float orbitSpeed_ = 0.3f;
	float orbitHeight_ = 4.0f;
	float orbitAngle_ = 0.0f;
};
