#pragma once
#include "../BaseCollider.h"

// Math
#include "MathFunc.h"


class AABBCollider : public BaseCollider
{
public:

	/*===============================================================//


							ポリモーフィズム


	//===============================================================*/
	~AABBCollider();
	void InitJson(JsonManager* jsonManager);
	Vector3 GetCenterPosition() const override;
	Vector3 GetScale() const override;
	Vector3 GetAnchorPoint() const override;
	Matrix4x4 GetWorldMatrix() const override;
	Vector3 GetEulerRotation() const override;


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

