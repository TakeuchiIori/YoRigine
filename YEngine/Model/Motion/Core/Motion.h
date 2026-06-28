#pragma once

#include <optional>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#include "../../Skeleton/Joint.h"
#include "../../Node/Node.h"
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

	using KeyframeVector3    = Keyframe<Vector3>;
	using KeyframeFloat      = Keyframe<float>;
	using KeyframeQuaternion = Keyframe<Quaternion>;

	template<typename tValue>
	struct AnimationCurve {
		std::vector<Keyframe<tValue>> keyframes;
	};

	struct NodeAnimation {
		AnimationCurve<Vector3>    translate;
		AnimationCurve<Quaternion> rotate;
		AnimationCurve<Vector3>    scale;
		InterpolationType          interpolationType;
	};

	// ---------------------------------------------------------------
	// タイムスケールカーブ
	// X = 正規化アニメーション時間 [0, 1]
	// Y = 再生速度倍率 (1.0 = 等速、2.0 = 2倍速、0.5 = 半速)
	// ---------------------------------------------------------------
	struct SpeedCurve {
		AnimationCurve<float> curve;   // keyframes: time=normalized_t, value=speed_multiplier

		bool IsEmpty() const { return curve.keyframes.empty(); }

		/// 正規化時間 t [0,1] での速度倍率を線形補間で返す。
		/// カーブが空の場合は 1.0f を返す。
		float Evaluate(float normalizedT) const
		{
			const auto& kfs = curve.keyframes;
			if (kfs.empty()) return 1.0f;
			if (normalizedT <= kfs.front().time) return kfs.front().value;
			if (normalizedT >= kfs.back().time)  return kfs.back().value;

			// 二分探索で区間を特定
			auto it = std::lower_bound(kfs.begin(), kfs.end(), normalizedT,
				[](const Keyframe<float>& kf, float t) { return kf.time < t; });

			if (it == kfs.begin()) return kfs.front().value;
			auto prev = std::prev(it);

			float range = it->time - prev->time;
			if (range < 1e-6f) return prev->value;
			float alpha = (normalizedT - prev->time) / range;
			return prev->value + alpha * (it->value - prev->value);
		}
	};

	struct AnimationModel {
		float duration_;
		std::map<std::string, NodeAnimation> nodeAnimations_;

		// タイムスケールカーブ（省略可能）
		SpeedCurve speedCurve;
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
	float      CalculateValue(const std::vector<KeyframeFloat>&      keyframes, float time, InterpolationType interpolationType);
	Vector3    CalculateValue(const std::vector<KeyframeVector3>&    keyframes, float time, InterpolationType interpolationType);
	Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time, InterpolationType interpolationType);

	// ============================================================
	// タイムスケールカーブ
	// ============================================================

	/// 正規化時間 t [0,1] での速度倍率を返す（カーブが空なら 1.0f）
	float EvaluateSpeedCurve(float normalizedT) const
	{
		return animation_.speedCurve.Evaluate(normalizedT);
	}

	bool HasSpeedCurve() const { return !animation_.speedCurve.IsEmpty(); }

	SpeedCurve&       GetSpeedCurve()       { return animation_.speedCurve; }
	const SpeedCurve& GetSpeedCurve() const { return animation_.speedCurve; }

	/// スピードカーブをクリアして等速に戻す
	void ClearSpeedCurve() { animation_.speedCurve.curve.keyframes.clear(); }

	// ============================================================
	// アクセッサ
	// ============================================================
	float GetDuration() const              { return animation_.duration_; }
	void  SetDuration(float duration)      { animation_.duration_ = duration; }

public:
	// ============================================================
	// メンバ変数
	// ============================================================
	AnimationModel animation_;
	Matrix4x4      localMatrix_;
	float          animationTime_ = 0.0f;
};
