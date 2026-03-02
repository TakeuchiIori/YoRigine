#pragma once
#include <Vector3.h>
#include <Matrix4x4.h>
#include "MathFunc.h"
#include <WorldTransform/WorldTransform.h>
#include "Loaders/Json/JsonManager.h"
#include "Object3D/Object3d.h"


/// <summary>
/// スプラインを使用してカメラ制御するクラス
/// </summary>
class SplineCamera
{
public:
	///************************* 基本的な関数 *************************///
	void Initialize();
	void InitJson();
	void Update();
	void FollowProsess();
	void Draw(Camera* camera);

	/// <summary>
	/// 制御点の登録
	/// </summary>
	void RegisterControlPoints();


public:

	// 移動完了時のコールバック関数を登録
	std::function<void()> isFinishedMove_ = nullptr;
	Vector3 translate_ = { 0,0,0 };
	Vector3 scale_ = { 1,1,1 };
	Vector3 rotate_ = { 0,0,0 };
	Matrix4x4 matView_ = {};
	void SetTarget(const WorldTransform& target) { target_ = &target; }
	float GetFov() const { return (fov_ >= 110.0f) ? fov_ : fov_; }

private:
	///************************* 内部処理 *************************///
	Vector3 EvaluateSpline(float t);

private:
	///************************* メンバ変数 *************************///
	std::unique_ptr <YoRigine::JsonManager> jsonManager_;
	std::vector<std::unique_ptr<Object3d>> obj_;
	std::vector<std::unique_ptr<WorldTransform>> wt_;
	std::vector<Vector3> controlPoints_;

	// 追従処理関連
	Vector3 rotation_;
	const WorldTransform* target_;
	Vector3 offset_ = { 0.0f, 6.0f, -40.0f };

	float fov_ = 0.90f;
	float t_ = 0.0f;
	float speed_ = 0.01f;
	bool hasCalledFinish_ = false;

};

