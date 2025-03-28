#pragma once
#include "../Collider.h"
#include "MathFunc.h"

class OBBCollider : public Collider
{
public:
	~OBBCollider() = default;
	Vector3 GetCenterPosition() const override {};
	Matrix4x4 GetWorldMatrix() const override {};
	
	void OnCollision([[maybe_unused]] Collider* other) override {};
	void EnterCollision([[maybe_unused]] Collider* other) override {};
	void ExitCollision([[maybe_unused]] Collider* other) override {};

	void Initialize();
	void Draw();

public:

	/// <summary>
	///  OBBを取得
	/// </summary>
	OBB GetOBB() const { return obb_; }
	/// <summary>
	///  OBBを設定
	/// </summary>
	void SetOBB(OBB obb) { obb_ = obb; }

private:
	OBB obb_;
};

