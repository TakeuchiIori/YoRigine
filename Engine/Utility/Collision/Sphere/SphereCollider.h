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

	void Draw(Camera* camera);
	
private:
	Sphere sphere_;

};

