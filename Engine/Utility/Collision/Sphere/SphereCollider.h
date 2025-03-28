#pragma once
#include "../Collider.h"
#include "MathFunc.h"

class SphereCollider : public Collider
{
public:
	/*===============================================================//
	

							  ポリモーフィズム


	//===============================================================*/

	~SphereCollider() = default;
	void InitJson(JsonManager* jsonManager) override;
	Vector3 GetCenterPosition() const override = 0;
	Matrix4x4 GetWorldMatrix() const override = 0;
	void OnCollision([[maybe_unused]] Collider* other) override = 0;
	void EnterCollision([[maybe_unused]] Collider* other) override = 0;
	void ExitCollision([[maybe_unused]] Collider* other) override = 0;

	/*===============================================================//


								基本的な関数


	//===============================================================*/

	void Initialize();
	void Update();
	void Draw();
	void InitJson();
public:

	/// <summary>
	///  球を取得
	/// </summary>
	Sphere GetSphere() const { return sphere_; }
	/// <summary>
	///  球を設定
	/// </summary>
	void SetSphere(Sphere sphere) { sphere_ = sphere; }

	float GetRadius() const { return sphere_.radius; }
	float GetRadius() { return sphere_.radius; }
private:
	Sphere sphere_;
	Sphere sphereOffset_;
	float radius_;
};

