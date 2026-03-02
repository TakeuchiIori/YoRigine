#pragma once

// C++
#include <optional>
#include <map>
#include <vector>

// Engine
#include "../Skeleton/Joint.h"
#include "../Node/Node.h"

// Math
#include "Quaternion.h"
#include "Vector3.h"

// モーションクラス
class Motion
{
public:
	///************************* 定義 *************************///

	// 補間タイプ
	enum class InterpolationType {
		Linear,
		Step,
		CubicSpline
	};

	// キーフレーム
	template <typename tValue>
	struct Keyframe {
		float time;
		tValue value;
	};

	// ベクトルのキーフレーム
	using KeyframeVector3 = Keyframe<Vector3>;
	using KeyframeFloat = Keyframe<float>;

	// 回転のキーフレーム
	using KeyframeQuaternion = Keyframe<Quaternion>;

	// アニメーションカーブ
	template<typename tValue>
	struct AnimationCurve {
		std::vector<Keyframe<tValue>> keyframes;
	};

	// ノード単位のアニメーション
	struct NodeAnimation {
		AnimationCurve<Vector3> translate;
		AnimationCurve<Quaternion> rotate;
		AnimationCurve<Vector3> scale;
		InterpolationType interpolationType;
	};

	// （0.0f~1.0f）に対する再生速度倍率のカーブ
	struct SpeedCurve {
		Motion::AnimationCurve<float> playbackSpeed;
	};

	// モーション全体のデータ
	struct AnimationModel {
		float duration_;
		std::map<std::string, NodeAnimation> nodeAnimations_;
		//SpeedCurve speedCurve;
	};

public:
	///************************* 基本関数 *************************///

	// GLTFから読み込み
	static Motion LoadFromScene(const aiScene* scene, const std::string& gltfFilePath, const std::string& animationName);

	// 補間タイプ解析
	static std::string ParseGLTFInterpolation(const std::string& gltfFilePath, uint32_t samplerIndex);

	// バイナリ保存
	void SaveBinary(const Motion& motion, const std::string& animationName, const std::string& path);

	// バイナリ読み込み
	Motion LoadBinary(const std::string& path);

	// アニメーション適用
	void ApplyAnimation(std::vector<Joint>& joints, float animationTime);

	// アニメーション再生
	void PlayerAnimation(float animationTime, Node& node);

	// ベクトル値取得
	Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time, InterpolationType interpolationType);
	float CalculateValue(const std::vector<KeyframeFloat>& keyframes, float time, InterpolationType interpolationType);
	// 回転値取得
	Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time, InterpolationType interpolationType);

public:
	///************************* アクセッサ *************************///

	// モーションの長さ取得
	float GetDuration() const { return animation_.duration_; }

	// モーションの長さ設定
	void SetDuration(float duration) { animation_.duration_ = duration; }

public:
	///************************* メンバ変数 *************************///

	// モーション全体のデータ
	AnimationModel animation_;

	// ローカル行列
	Matrix4x4 localMatrix_;

	// 再生時間
	float animationTime_ = 0.0f;
};
