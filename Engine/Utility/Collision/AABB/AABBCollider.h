#pragma once
#include "../Core/BaseCollider.h"

// Math
#include "MathFunc.h"


class AABBCollider : public BaseCollider
{
public:

	/*===============================================================//


							ポリモーフィズム


	//===============================================================*/
	~AABBCollider() = default;
	void InitJson(JsonManager* jsonManager) override;
	Vector3 GetCenterPosition() const override;
	const WorldTransform& GetWorldTransform() override;
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

