#pragma once

#include "Base/BaseArea.h"

// 円形エリアを扱うクラス
// XZ平面上の円形で判定を行い、Y軸方向は制限しない
class CircleArea : public BaseArea
{
public:
	///************************* コンストラクタ *************************///

	CircleArea() = default;
	CircleArea(const Vector3& center, float radius);

public:
	///************************* 基本関数 *************************///

	// 初期化
	void Initialize(const Vector3& center, float radius);

public:
	///************************* BaseArea実装 *************************///

	// エリア内かどうかを判定
	bool IsInside(const Vector3& position) const override;

	// 位置をエリア内にクランプ
	Vector3 ClampPosition(const Vector3& position) const override;

	// エリア境界までの距離を取得
	float GetDistanceFromBoundary(const Vector3& position) const override;

	// エリアの中心座標を取得
	Vector3 GetCenter() const override { return center_; }

	// デバッグ描画
	void Draw(Line* line) override;

	// エリアタイプを取得
	AreaType GetAreaType() const override { return AreaType::Circle; }

public:
	///************************* アクセッサ *************************///

	void SetCenter(const Vector3& center) { center_ = center; }
	void SetRadius(float radius) { radius_ = radius; }

	float GetRadius() const { return radius_; }

public:
	///************************* デバッグ設定 *************************///

	// デバッグ描画の分割数設定
	void SetDebugSegments(int segments) { debugSegments_ = segments; }

private:
	///************************* メンバ変数 *************************///

	Vector3 center_;     // 円の中心座標
	float radius_;       // 円の半径
	int debugSegments_;  // デバッグ描画の分割数
};