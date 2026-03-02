#include "BaseArea.h"
#include "MathFunc.h"

void BaseArea::Update(const Vector3& targetPosition)
{
	if (!isActive_) {
		return;
	}

	bool currentlyInside = IsInside(targetPosition);

	// エリアに入った瞬間
	if (currentlyInside && !wasInside_) {
		if (enterCallback_) {
			enterCallback_(targetPosition);
		}
	}
	// エリアから出た瞬間
	else if (!currentlyInside && wasInside_) {
		if (exitCallback_) {
			exitCallback_(targetPosition);
		}
	}
	// エリア内にいる間
	else if (currentlyInside && wasInside_) {
		if (stayCallback_) {
			stayCallback_(targetPosition);
		}
	}

	wasInside_ = currentlyInside;
}

bool BaseArea::IsTouchingBoundary(const Vector3& position, float margin) const
{
	if (!isActive_) {
		return false;
	}

	float distanceFromBoundary = GetDistanceFromBoundary(position);
	return distanceFromBoundary <= margin && distanceFromBoundary >= 0.0f;
}

Vector3 BaseArea::GetPushBackVector(const Vector3& position) const
{
	if (!isActive_) {
		return Vector3(0.0f, 0.0f, 0.0f);
	}

	// エリア内なら押し戻し不要
	if (IsInside(position)) {
		return Vector3(0.0f, 0.0f, 0.0f);
	}

	// 境界内の位置を取得
	Vector3 clampedPos = ClampPosition(position);

	// 現在位置からクランプ位置へのベクトルが押し戻しベクトル
	return clampedPos - position;
}

Vector3 BaseArea::SmoothClampPosition(const Vector3& currentPos,
	const Vector3& targetPos,
	float lerpFactor) const
{
	if (!isActive_) {
		return targetPos;
	}

	Vector3 clampedTarget = ClampPosition(targetPos);

	// 現在位置とクランプされた目標位置を補間
	return Lerp(currentPos, clampedTarget, lerpFactor);
}