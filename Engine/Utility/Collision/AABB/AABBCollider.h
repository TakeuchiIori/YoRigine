#pragma once
#include "../Collider.h"

// Math
#include "MathFunc.h"
#include "Matrix4x4.h"
#include "Object3D/Object3d.h"

class AABBCollider : public Collider
{
public:

	~AABBCollider() = default;

	void Initialzie();

	Vector3 GetCenterPosition() const override {};
	Matrix4x4 GetWorldMatrix() const override {};
	void OnCollision([[maybe_unused]] Collider* other) override {};
	void EnterCollision([[maybe_unused]] Collider* other) override {};
	void ExitCollision([[maybe_unused]] Collider* other) override {};

	void Draw(Camera* camera);

public: // アクセッサ

	/// <summary>
	///  AABBを取得
	/// </summary>
	AABB GetAABB() const { return aabb_; }

	/// <summary>
	///  AABBを設定
	/// </summary>
	void SetAABB(AABB aabb) { aabb_ = aabb; }

private:
	AABB aabb_;

};

