#pragma once

// Engine
#include "../Core/BaseCollider.h"

// Math
#include "MathFunc.h"

// 球体コライダーを扱うクラス
class SphereCollider : public BaseCollider
{
public:
	///************************* ポリモーフィズム *************************///

	~SphereCollider() = default;
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

	// JSON初期化
	void InitJson();

public:
	///************************* アクセッサ *************************///

	// 球体取得
	Sphere GetSphere() const { return sphere_; }

	// 球体設定
	void SetSphere(Sphere sphere) { sphere_ = sphere; }

	// 半径取得
	float GetRadius() const { return sphere_.radius; }

	// 半径設定
	void SetRadius(float radius) { radius_ = radius; }

public:
	///************************* 調整用 *************************///

	Sphere sphereOffset_;

private:
	///************************* メンバ変数 *************************///

	Sphere sphere_;
	float radius_;
};
