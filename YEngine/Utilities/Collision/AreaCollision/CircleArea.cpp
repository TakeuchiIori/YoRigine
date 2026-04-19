#include "CircleArea.h"
#include "MathFunc.h"
#include <cmath>

// ============================================================
// コンストラクタ
// ============================================================
CircleArea::CircleArea(const Vector3& center, float radius)
	: center_(center)
	, radius_(radius)
	, debugSegments_(64)
{
}

// ============================================================
// 初期化
// ============================================================
void CircleArea::Initialize(const Vector3& center, float radius)
{
	center_ = center;
	radius_ = radius;
	debugSegments_ = 64;
	isActive_ = true;
	wasInside_ = false;
}

// ============================================================
// エリア内かどうかを判定
// ============================================================
bool CircleArea::IsInside(const Vector3& position) const
{
	// 高さが範囲外ならfalse
	if (position.y < ground_.bottom || position.y > ground_.top) {
		return false;
	}

	// XZ平面での距離を計算
	Vector3 toPosition = position - center_;
	toPosition.y = 0.0f;

	float distanceSq = LengthSquared(toPosition);
	return distanceSq <= (radius_ * radius_);
}

// ============================================================
// 位置をエリア内にクランプ
// ============================================================
Vector3 CircleArea::ClampPosition(const Vector3& position) const
{
	Vector3 result = position;
	// 床より下に行こうとしたら床の高さで止める
	if (result.y < ground_.bottom) {
		result.y = ground_.bottom;
	}else if(result.y > ground_.top) {
		result.y = ground_.top;
	}

	// XZ（横）のはみ出し補正
	Vector3 toPosition = position - center_;
	toPosition.y = 0.0f;
	float distance = Length(toPosition);
	if (distance > radius_) {
		Vector3 direction = Normalize(toPosition);
		Vector3 clampedXZ = center_ + direction * radius_;
		result.x = clampedXZ.x;
		result.z = clampedXZ.z;
	}


	return result;
}

// ============================================================
// エリア境界までの距離を取得
// ============================================================
float CircleArea::GetDistanceFromBoundary(const Vector3& position) const
{
	// 側面の境界までの距離
	Vector3 toPosition = position - center_;
	toPosition.y = 0.0f;
	float horizontalDistance = Length(toPosition);
	float distToSide = radius_ - horizontalDistance;

	// 床までの距離
	float distToFloor = position.y - ground_.bottom;

	// 天井までの距離
	float distToCeiling = ground_.top - position.y;

	// 側面、床、天井のうち、「一番近い距離」を返す
	// ※どれか1つでもマイナス（範囲外）になっていれば、その一番深いマイナス値が返る
	return std::min({ distToSide, distToFloor, distToCeiling });
}

// ============================================================
// デバッグ描画
// ============================================================
void CircleArea::Draw(Line* line)
{
	if (!line || !isDebugDrawEnabled_) {
		return;
	}
	line->DrawCircleXZ(center_, radius_, 256);
}