#pragma once

// Engine
#include "../Core/BaseCollider.h"

// Math
#include "MathFunc.h"
#include "Shape/Capsule.h"

// カプセルコライダー (人型キャラ等で使う立位カプセル)
//
// ローカル軸は既定で +Y。WorldTransform を持つオブジェクトの位置を中心とし、
// 半長 (halfHeight) と半径 (radius) で形状が決まる。
// オフセット (centerOffset) でワールド中心からズラせる。
//
// half-height は「円柱部分の半長」。両端の半球は半径分だけさらに伸びる。
//   total height = halfHeight * 2 + radius * 2
class CapsuleCollider : public BaseCollider
{
public:
	///************************* ポリモーフィズム *************************///

	~CapsuleCollider() = default;
	void InitJson(YoRigine::JsonManager* jsonManager) override;
	Vector3 GetCenterPosition() const override;
	const WorldTransform& GetWorldTransform() override;
	Vector3 GetEulerRotation() const override;

public:
	///************************* 基本関数 *************************///

	void Initialize();
	void Update() override;
	void Draw() override;

public:
	///************************* アクセッサ *************************///

	const Capsule& GetCapsule() const { return capsule_; }

	void SetHalfHeight(float h) { halfHeight_ = h; }
	void SetRadius(float r)     { radius_     = r; }
	void SetCenterOffset(const Vector3& o) { centerOffset_ = o; }
	void SetAxisLocal(const Vector3& a)    { axisLocal_    = a; }

	float          GetHalfHeight()  const { return halfHeight_; }
	float          GetRadius()      const { return radius_; }
	const Vector3& GetCenterOffset()const { return centerOffset_; }
	const Vector3& GetAxisLocal()   const { return axisLocal_; }

private:
	///************************* 構成 *************************///

	float   halfHeight_    = 1.0f;            // 円柱部の半長
	float   radius_        = 0.5f;            // 半径
	Vector3 centerOffset_  = { 0, 0, 0 };     // WorldTransform中心からのローカルオフセット
	Vector3 axisLocal_     = { 0, 1, 0 };     // ローカル軸方向 (規定 +Y)

	///************************* 算出結果 *************************///

	Capsule capsule_{};
};
