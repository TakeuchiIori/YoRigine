#pragma once
#include "../Collider.h"
#include "MathFunc.h"

class SphereCollider : public Collider
{
public:
	~SphereCollider() = default;
	Vector3 GetCenterPosition() const override {
		return sphere_.center;
	}
	Matrix4x4 GetWorldMatrix() const override {
		return MakeIdentity4x4();
	}
	void OnCollision([[maybe_unused]] Collider* other) override {};
	void EnterCollision([[maybe_unused]] Collider* other) override {};
	void ExitCollision([[maybe_unused]] Collider* other) override {};

	void Initialize();
	void Draw(Camera* camera);
	
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

};

