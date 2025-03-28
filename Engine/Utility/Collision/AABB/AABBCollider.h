#pragma once
#include "../Collider.h"

// Math
#include "MathFunc.h"


class AABBCollider : public Collider
{
public:

	/*===============================================================//


							ポリモーフィズム


	//===============================================================*/
	~AABBCollider() = default;
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
	AABB aabbOffset_;

};

