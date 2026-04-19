#pragma once

#include <optional>
#include <map>
#include <vector>
#include <string>

#include "../Skeleton/Joint.h"
#include "../Node/Node.h"
#include "Quaternion.h"
#include "Vector3.h"

// assimp
struct aiScene;

// ============================================================
// モーションクラス
// アニメーションデータを保持・管理・適用する
// ============================================================
class Motion
{
public:
	// ============================================================
	// 構造体・列挙型定義
	// ============================================================
	enum class InterpolationType {
		Linear,
		Step,
		CubicSpline
	};

	template <typename tValue>
	struct Keyframe {
		float time;
		tValue value;
	};

	using KeyframeVector3 = Keyframe<Vector3>;
	using KeyframeFloat = Keyframe<float>;
	using KeyframeQuaternion = Keyframe<Quaternion>;

	template<typename tValue>
	struct AnimationCurve {
		std::vector<Keyframe<tValue>> keyframes;
	};

	struct NodeAnimation {
		AnimationCurve<Vector3> translate;
		AnimationCurve<Quaternion> rotate;
		AnimationCurve<Vector3> scale;
		InterpolationType interpolationType;
	};

	struct SpeedCurve {
		AnimationCurve<float> playbackSpeed;
	};

	struct AnimationModel {
		float duration_;
		std::map<std::string, NodeAnimation> nodeAnimations_;
	};

public:
	// ============================================================
	// 基本関数（ロード・セーブ・適用）
	// ============================================================
	static Motion LoadFromScene(const aiScene* scene, const std::string& gltfFilePath, const std::string& animationName);
	static std::string ParseGLTFInterpolation(const std::string& gltfFilePath, uint32_t samplerIndex);

	void SaveBinary(const Motion& motion, const std::string& animationName, const std::string& path);
	Motion LoadBinary(const std::string& path);

	void ApplyAnimation(std::vector<Joint>& joints, float animationTime);
	void PlayerAnimation(float animationTime, Node& node);

	// ============================================================
	// キーフレーム計算
	// ============================================================
	float CalculateValue(const std::vector<KeyframeFloat>& keyframes, float time, InterpolationType interpolationType);
	Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time, InterpolationType interpolationType);
	Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time, InterpolationType interpolationType);

public:
	// ============================================================
	// アクセッサ
	// ============================================================
	float GetDuration() const { return animation_.duration_; }
	void SetDuration(float duration) { animation_.duration_ = duration; }

public:
	// ============================================================
	// メンバ変数
	// ============================================================
	AnimationModel animation_;
	Matrix4x4 localMatrix_;
	float animationTime_ = 0.0f;
};