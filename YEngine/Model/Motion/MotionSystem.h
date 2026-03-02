#pragma once
#include "Motion.h"
#include "../Skeleton/Skeleton.h"
#include "../Skeleton/SkinCluster.h"
#include "../Node/Node.h"
#include "Quaternion.h"

#include <functional>
#include <unordered_map>

// アニメーション再生モード
enum class MotionPlayMode {
	Stop,   // 停止
	Once,   // 一回再生
	Loop    // 無限ループ再生
};

// アニメーションシステム
class MotionSystem {
public:
	///************************* 基本関数 *************************///

	// 初期化
	void Initialize(Motion& motion, Skeleton& skeleton, SkinCluster& skinCluster, Node* node);
	void Initialize(Motion& motion, Node* rootNode);

	// 更新
	void Update(float deltaTime);

	// アニメーション適用
	void Apply();

	// 再生制御
	void PlayOnce();
	void PlayLoop();
	void Stop();
	void Resume();

	// モーションブレンド開始
	void StartBlend(Motion& toAnimation, float blendDuration);

	///************************* コールバック *************************///

	// モーションが終了したとき
	void SetOnMotionFinishedCallback(const std::function<void()>& callback) {
		onMationFinished_ = callback;
	}
	std::function<void()> onMationFinished_;

public:
	///************************* アクセッサ *************************///

	// 正規化されたノード名取得
	std::string GetNormalizedName(const std::string& name);

	// 指定ノードのトランスフォーム取得
	QuaternionTransform GetTransformAnimation(const Motion& anim, const std::string& nodeName, float time);

	// 再生モード設定
	void SetPlayMode(MotionPlayMode playMode);

	// 再生完了確認
	bool IsFinished() const { return isFinished_; }

	// モーション速度
	float GetMotionSpeed() const { return motionSpeed_; }
	void SetMotionSpeed(float speed) { motionSpeed_ = speed; }

	// 現在のアニメーション速度
	void SetCurrentAnimationSpeed(float speed) { currentAnimationSpeed_ = speed; }
	float GetCurrentAnimationSpeed() const { return currentAnimationSpeed_; }

	// 実際の再生速度
	float GetEffectiveSpeed() const { return motionSpeed_ * currentAnimationSpeed_; }

private:
	///************************* 内部処理 *************************///

	// アニメーション補間と適用
	void BlendAndApplyAnimation(const Motion& from, const Motion& to, float t);

private:
	///************************* メンバ変数 *************************///

	// アニメーションデータ
	Motion* animation_ = nullptr;

	// スケルトンデータ
	Skeleton* skeleton_ = nullptr;

	// スキンクラスター
	SkinCluster* skinCluster_ = nullptr;

	// ノードデータ
	Node* node_ = nullptr;

	// アニメーション時間
	float animationTime_ = 0.0f;

	// ブレンド状態
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

	// ノード名キャッシュ
	std::unordered_map<std::string, std::string> normalizedNameCache_;

	// 再生モード
	MotionPlayMode playMode_ = MotionPlayMode::Loop;

	// 前回の再生モード
	MotionPlayMode prevPlayMode_ = MotionPlayMode::Loop;

	// 再生状態
	bool isFinished_ = false;

	// モーション速度
	float motionSpeed_ = 1.0f;

	// 現在のアニメーション速度
	float currentAnimationSpeed_ = 1.0f;
};
