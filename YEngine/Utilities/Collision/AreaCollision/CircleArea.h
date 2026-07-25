#pragma once

#include "Base/BaseArea.h"

// 円形エリアを扱うクラス
// XZ平面上の円形で判定を行い、Y軸方向は ground の top/bottom で制限する
class CircleArea : public BaseArea
{
public:
	///************************* コンストラクタ *************************///

	CircleArea();
	CircleArea(const Vector3& center, float radius);

public:
	///************************* 基本関数 *************************///

	void Initialize(const Vector3& center, float radius);

public:
	///************************* BaseArea 実装 *************************///

	bool     IsInside(const Vector3& position) const override;
	Vector3  ClampPosition(const Vector3& position) const override;
	float    GetDistanceFromBoundary(const Vector3& position) const override;
	Vector3  GetCenter() const override             { return center_; }
	void     Draw(YoRigine::Line* line) override;
	AreaType GetAreaType() const override           { return AreaType::Circle; }

	// JSON 保存・ファクトリ用
	std::string GetTypeString() const override      { return "Circle"; }

public:
	///************************* アクセッサ *************************///

	void  SetCenter(const Vector3& center)  { center_ = center; }
	void  SetRadius(float radius)           { radius_ = radius; }
	float GetRadius() const                 { return radius_; }
	void  SetGroundBottom(float bottom)     { ground_.bottom = bottom; }
	void  SetGroundTop(float top)           { ground_.top    = top; }
	void  SetDebugSegments(int segments)    { debugSegments_ = segments; }

protected:
	///************************* AutoJson 登録 *************************///

	void SetupAutoJson() override
	{
		BaseArea::SetupAutoJson();            // active, debugDraw
		aj_.Add("center",       &center_)
		   .Add("radius",       &radius_)
		   .Add("groundBottom", &ground_.bottom)
		   .Add("groundTop",    &ground_.top);
	}

private:
	///************************* メンバ変数 *************************///

	struct Ground {
		float top    = 100.0f;
		float bottom = 0.0f;
	} ground_;

	Vector3 center_       = { 0.0f, 0.0f, 0.0f };
	float   radius_       = 10.0f;
	int     debugSegments_ = 64;
};
