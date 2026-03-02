#pragma once

// Engine
#include "../Core/BaseCollider.h"

// Math
#include "MathFunc.h"

// AABBコライダーを扱うクラス
class AABBCollider : public BaseCollider
{
public:
	///************************* ポリモーフィズム *************************///

	~AABBCollider() = default;
	void InitJson(YoRigine::JsonManager* jsonManager) override;
	Vector3 GetCenterPosition() const override;
	const WorldTransform& GetWorldTransform() override;
	Vector3 GetEulerRotation() const override;

public:
	///************************* 基本関数 *************************///

	// 初期化
	void Initialize();

	// 更新
	void Update();

	// 描画
	void Draw();

public:
	///************************* アクセッサ *************************///

	// AABB取得
	AABB GetAABB() const { return aabb_; }

	// AABB設定
	void SetAABB(AABB aabb) { aabb_ = aabb; }

public:
	///************************* 調整用 *************************///

	AABB aabbOffset_;

private:
	///************************* メンバ変数 *************************///

	AABB aabb_;
};
