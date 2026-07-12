#include "BaseArea.h"
#include "MathFunc.h"

void BaseArea::Update(const Vector3& targetPosition, void* targetKey, float deltaTime)
{
	if (!isActive_) {
		return;
	}

	const bool currentlyInside = IsInside(targetPosition);
	const auto it = insideTargets_.find(targetKey);
	const bool wasInside = (it != insideTargets_.end());

	if (currentlyInside && !wasInside) {
		insideTargets_.insert(targetKey);
		stayTimers_[targetKey] = 0.0f;           // 進入時に tick タイマーをリセット
		if (enterCallback_) enterCallback_(targetKey, targetPosition);
	}
	else if (!currentlyInside && wasInside) {
		insideTargets_.erase(it);
		stayTimers_.erase(targetKey);
		if (exitCallback_) exitCallback_(targetKey, targetPosition);
	}
	else if (currentlyInside && wasInside) {
		if (!stayCallback_) return;

		// tick 間隔が未設定なら毎フレーム発火（従来動作）。
		if (stayTickInterval_ <= 0.0f) {
			stayCallback_(targetKey, targetPosition);
			return;
		}

		// 間隔ぶん経過するごとに Stay を発火（毒などの継続ダメージ tick）。
		float& elapsed = stayTimers_[targetKey];
		elapsed += deltaTime;
		while (elapsed >= stayTickInterval_) {
			elapsed -= stayTickInterval_;
			stayCallback_(targetKey, targetPosition);
		}
	}
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
