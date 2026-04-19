#pragma once
#include "Motion.h"
#include "../Skeleton/Skeleton.h"
#include "../Skeleton/SkinCluster.h"
#include "../Node/Node.h"
#include "Quaternion.h"

#include <functional>
#include <unordered_map>

// ============================================================
// アニメーション再生モード
// ============================================================
enum class MotionPlayMode {
	Stop,   // 停止
	Once,   // 一回再生
	Loop    // 無限ループ再生
};

// ============================================================
// アニメーションシステムクラス
// モーションの再生、ブレンド、適用を管理する
// ============================================================
class MotionSystem {
public:
	// ============================================================
	// 基本関数
	// ============================================================
	void Initialize(Motion& motion, Skeleton& skeleton, SkinCluster& skinCluster, Node* node);
	void Initialize(Motion& motion, Node* rootNode);
	void Update(float deltaTime);
	void Apply();

	// ============================================================
	// 再生制御
	// ============================================================
	void PlayOnce();
	void PlayLoop();
	void Stop();
	void Resume();
	void StartBlend(Motion& toAnimation, float blendDuration);

	// ============================================================
	// コールバック
	// ============================================================
	void SetOnMotionFinishedCallback(const std::function<void()>& callback) {
		onMotionFinished_ = callback;
	}
	std::function<void()> onMotionFinished_;

public:
	// ============================================================
	// アクセッサ
	// ============================================================
	std::string GetNormalizedName(const std::string& name);
	QuaternionTransform GetTransformAnimation(const Motion& anim, const std::string& nodeName, float time);

	void SetPlayMode(MotionPlayMode playMode);
	bool IsFinished() const { return isFinished_; }

	float GetMotionSpeed() const { return motionSpeed_; }
	void SetMotionSpeed(float speed) { motionSpeed_ = speed; }

	void SetCurrentAnimationSpeed(float speed) { currentAnimationSpeed_ = speed; }
	float GetCurrentAnimationSpeed() const { return currentAnimationSpeed_; }

	float GetEffectiveSpeed() const { return motionSpeed_ * currentAnimationSpeed_; }

	void SetAnimationTime(float time);
	float GetAnimationTime() const { return animationTime_; }
	float GetDuration() const { return animation_ ? animation_->GetDuration() : 0.0f; }

	Motion* GetAnimation() const { return animation_; }

private:
	// ============================================================
	// 内部処理
	// ============================================================
	void BlendAndApplyAnimation(const Motion& from, const Motion& to, float t);

private:
	// ============================================================
	// メンバ変数
	// ============================================================
	Motion* animation_ = nullptr;
	Skeleton* skeleton_ = nullptr;
	SkinCluster* skinCluster_ = nullptr;
	Node* node_ = nullptr;

	float animationTime_ = 0.0f;

	struct AnimationBlendState {
		Motion from;
		Motion to;
		float fromTime = 0.0f;
		float toTime = 0.0f;
		float blendTime = 0.0f;
		float currentTime = 0.0f;
		bool isBlending = false;
	};

	AnimationBlendState animationBlendState_;

	std::unordered_map<std::string, std::string> normalizedNameCache_;

	MotionPlayMode playMode_ = MotionPlayMode::Loop;
	MotionPlayMode prevPlayMode_ = MotionPlayMode::Loop;
	bool isFinished_ = false;

	float motionSpeed_ = 1.0f;
	float currentAnimationSpeed_ = 1.0f;
};