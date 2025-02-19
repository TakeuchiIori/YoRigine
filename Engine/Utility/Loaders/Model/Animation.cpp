#include "Animation.h"

#include "MathFunc.h"
#include <assert.h>

void Animation::ApplyAnimation(std::vector<Joint>& joints)
{
	for (Joint& joint :joints) {
		// 対象のJointのAnimationがあれば、値の適用を行う。
		// 下記のif文はC++17から可能になったinit-statement付きのif文。
		if (auto it = animation_.nodeAnimations_.find(joint.GetName()); it != animation_.nodeAnimations_.end()) {
			const NodeAnimation& rootNodeAnimation = (*it).second;
			QuaternionTransform transform;
			transform.translate = CalculateValue(rootNodeAnimation.translate, animationTime_); // 指定時刻の値を取得
			transform.rotate = CalculateValue(rootNodeAnimation.rotate, animationTime_);
			transform.scale = CalculateValue(rootNodeAnimation.scale, animationTime_);
			joint.SetTransform(transform);
		}
	}
}

Vector3 Animation::CalculateValue(const AnimationCurve<Vector3>& curve, float time)
{
	assert(!curve.keyframes.empty());
	if (curve.keyframes.size() == 1 || time <= curve.keyframes[0].time) {
		return curve.keyframes[0].value;
	}

	for (size_t index = 0; index < curve.keyframes.size() - 1; ++index) {
		size_t nextIndex = index + 1;
		// indexとnextIndexの2つのkeyframeを取得して範囲内に時刻があるか判定
		if (curve.keyframes[index].time <= time && time <= curve.keyframes[nextIndex].time) {
			// 範囲内を補間する
			float t = (time - curve.keyframes[index].time) / (curve.keyframes[nextIndex].time - curve.keyframes[index].time);
			return Lerp(curve.keyframes[index].value, curve.keyframes[nextIndex].value, t);
		}
	}
	// ここまできた場合は一番後の時刻よりも後ろなので最後の値を返すことになる
	return (*curve.keyframes.rbegin()).value;
}

Quaternion Animation::CalculateValue(const AnimationCurve<Quaternion>& curve, float time)
{
	assert(!curve.keyframes.empty());
	if (curve.keyframes.size() == 1 || time <= curve.keyframes[0].time) {
		return curve.keyframes[0].value;
	}

	for (size_t index = 0; index < curve.keyframes.size() - 1; ++index) {
		size_t nextIndex = index + 1;
		// indexとnextIndexの2つのkeyframeを取得して範囲内に時刻があるか判定
		if (curve.keyframes[index].time <= time && time <= curve.keyframes[nextIndex].time) {
			// 範囲内を補間する
			float t = (time - curve.keyframes[index].time) / (curve.keyframes[nextIndex].time - curve.keyframes[index].time);
			return Lerp(curve.keyframes[index].value, curve.keyframes[nextIndex].value, t);
		}
	}
	// ここまできた場合は一番後の時刻よりも後ろなので最後の値を返すことになる
	return (*curve.keyframes.rbegin()).value;
}