#pragma once

// C++
#include <optional>
#include <map>
#include <vector>


// Engine
#include "Joint.h"
#include "Node.h"


// Math
#include "Quaternion.h"
#include "Vector3.h"



class Animation
{
private:
	template <typename tValue>
	struct Keyframe {
		float time;
		tValue value;
	};
	using KeyframeVector3 = Keyframe<Vector3>;
	using KeyframeQuaternion = Keyframe<Quaternion>;

	template<typename tValue>
	struct AnimationCurve {
		std::vector<Keyframe<tValue>> keyframes;
	};

	struct NodeAnimation {
		AnimationCurve<Vector3> translate;
		AnimationCurve<Quaternion> rotate;
		AnimationCurve<Vector3> scale;
	};

	struct AnimationModel {
		float duration_;										 // アニメーション全体の尺（秒）
		std::map<std::string, NodeAnimation> nodeAnimations_;	 // NodeAnimationの集合。Node名で開けるように
	};

public:



	/// <summary>
	/// アニメーション適用
	/// </summary>
	/// <param name="skeleton"></param>
	/// <param name="animation"></param>
	/// <param name="animationTime"></param>
	void ApplyAnimation(std::vector<Joint>& joints);


private:

	/// <summary>
	/// 任意の時刻を取得
	/// </summary>
	Vector3 CalculateValue(const AnimationCurve<Vector3>& curve, float time);

	/// <summary>
	/// 任意の時刻を取得
	/// </summary>
	Quaternion CalculateValue(const AnimationCurve<Quaternion>& curve, float time);

private:


	AnimationModel animation_;
	Matrix4x4 localMatrix_;
	float animationTime_ = 0.0f;

};

