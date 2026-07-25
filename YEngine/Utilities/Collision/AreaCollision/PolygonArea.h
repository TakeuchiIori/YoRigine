#pragma once

#include "Base/BaseArea.h"
#include <vector>

// 任意多角形エリアを扱うクラス
// XZ 平面上に頂点列でポリゴンを定義し Ray Casting 法で内外判定を行う
// Y 軸方向は groundBottom / groundTop で制限する
class PolygonArea : public BaseArea
{
public:
	///************************* コンストラクタ *************************///

	PolygonArea();
	explicit PolygonArea(const std::vector<Vector3>& vertices);

public:
	///************************* 基本関数 *************************///

	void Initialize(const std::vector<Vector3>& vertices);

public:
	///************************* BaseArea 実装 *************************///

	bool     IsInside(const Vector3& position) const override;
	Vector3  ClampPosition(const Vector3& position) const override;
	float    GetDistanceFromBoundary(const Vector3& position) const override;
	Vector3  GetCenter() const override;
	void     Draw(YoRigine::Line* line) override;
	AreaType GetAreaType() const override           { return AreaType::Polygon; }

	// JSON 保存・ファクトリ用
	std::string GetTypeString() const override      { return "Polygon"; }

public:
	///************************* 頂点アクセッサ *************************///

	void SetVertices(const std::vector<Vector3>& v) { vertices_ = v; }
	std::vector<Vector3>&       GetVertices()        { return vertices_; }
	const std::vector<Vector3>& GetVertices() const  { return vertices_; }

	void AddVertex(const Vector3& v)                 { vertices_.push_back(v); }
	void RemoveVertex(int index);

	void  SetGroundBottom(float v)  { groundBottom_ = v; }
	void  SetGroundTop(float v)     { groundTop_    = v; }
	float GetGroundBottom() const   { return groundBottom_; }
	float GetGroundTop() const      { return groundTop_; }

protected:
	///************************* AutoJson 登録 *************************///

	void SetupAutoJson() override
	{
		BaseArea::SetupAutoJson();              // active, debugDraw
		aj_.Add("groundBottom", &groundBottom_)
		   .Add("groundTop",    &groundTop_)
		   .Add("vertices",     &vertices_);    // vector<Vector3> も自動シリアライズ
	}

private:
	///************************* 内部ヘルパー *************************///

	// 線分 a-b 上で p に最も近い点を返す（XZ 平面）
	Vector3 ClosestPointOnSegment(const Vector3& p,
		const Vector3& a,
		const Vector3& b) const;

private:
	///************************* メンバ変数 *************************///

	std::vector<Vector3> vertices_;
	float groundBottom_ = 0.0f;
	float groundTop_    = 100.0f;
};
