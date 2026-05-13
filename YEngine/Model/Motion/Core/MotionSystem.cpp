#include "MotionSystem.h"

#include "../../ModelUtils.h"
#include "Debugger/Logger.h"

#include <Windows.h>
#include <unordered_set>
#include <algorithm>
#include <stdexcept>

#include "Vector3.h"
#include "Quaternion.h"

// ============================================================
// 初期化（スケルトンあり）
// ============================================================
void MotionSystem::Initialize(Motion& Motion, Skeleton& skeleton, SkinCluster& skinCluster, Node* node){
	animation_ = &Motion;
	skeleton_ = &skeleton;
	skinCluster_ = &skinCluster;
	node_ = node;
	animationTime_ = 0.0f;
}

// ============================================================
// 初期化（ノードのみ）
// ============================================================
void MotionSystem::Initialize(Motion& Motion, Node* rootNode){
	animation_ = &Motion;
	node_ = rootNode;
	animationTime_ = 0.0f;
}

// ============================================================
// 更新処理
// ============================================================
void MotionSystem::Update(float deltaTime){
	if (!animation_ || playMode_ == MotionPlayMode::Stop || isFinished_) return;

	float effectiveSpeed = GetEffectiveSpeed();

	// ------------------------------------------------------------
	// 上半身（アクション）レイヤーの更新
	// ------------------------------------------------------------
	if (upperAnimation_ && upperPlayMode_ != MotionPlayMode::Stop && !isUpperFinished_) {
		float upDuration = upperAnimation_->GetDuration();
		float upSpeedMul = 1.0f;
		if (upperAnimation_->HasSpeedCurve() && upDuration > 0.0f) {
			upSpeedMul = upperAnimation_->EvaluateSpeedCurve(std::clamp(upperAnimationTime_ / upDuration, 0.0f, 1.0f));
		}

		upperAnimationTime_ += deltaTime * upperMotionSpeed_ * upSpeedMul;

		if (upperAnimationTime_ >= upDuration) {
			if (upperPlayMode_ == MotionPlayMode::Loop) {
				upperAnimationTime_ = 0.0f;
			}
			else {
				upperAnimationTime_ = upDuration;
				isUpperFinished_ = true;
				StopUpperAnimation(); // 終わったら自動でストップ（下半身の動きに同期させるため）
			}
		}
	}

	// ------------------------------------------------------------
	// ベースとなるブレンド中の処理
	// ------------------------------------------------------------
	if (animationBlendState_.isBlending) {
		animationBlendState_.currentTime += deltaTime;

		if (animationBlendState_.currentTime >= animationBlendState_.blendTime) {
			animationBlendState_.isBlending = false;
			animationTime_ = animationBlendState_.toTime + animationBlendState_.currentTime;
		}
		return;
	}

	// ------------------------------------------------------------
	// ベースレイヤーの通常更新
	// ------------------------------------------------------------
	if (!animation_ || playMode_ == MotionPlayMode::Stop || isFinished_) return;

	bool wasFinished = isFinished_;
	float duration = animation_->GetDuration();
	float speedMul = 1.0f;
	if (animation_->HasSpeedCurve() && duration > 0.0f) {
		speedMul = animation_->EvaluateSpeedCurve(std::clamp(animationTime_ / duration, 0.0f, 1.0f));
	}
	animationTime_ += deltaTime * effectiveSpeed * speedMul;

	if (animationTime_ >= duration) {
		if (playMode_ == MotionPlayMode::Loop) animationTime_ = 0.0f;
		else { animationTime_ = duration; isFinished_ = true; }
	}
	else if (animationTime_ < 0.0f) {
		if (playMode_ == MotionPlayMode::Loop) animationTime_ = duration;
		else { animationTime_ = 0.0f; isFinished_ = true; }
	}

	if (!wasFinished && isFinished_ && onMotionFinished_) {
		onMotionFinished_();
	}
}

// ============================================================
// アニメーションの適用（合成する場所）
// ============================================================
// MotionSystem.cpp の Apply() 関数をこれで丸ごと上書き！

void MotionSystem::Apply()
{
	if (skeleton_) {
		for (Joint& joint : skeleton_->GetJoints()) {
			std::string normName = GetNormalizedName(joint.GetName());

			// マスクに登録されているか（上半身かどうか）の判定
			bool isUpperBody = (upperBodyBoneMask_.count(joint.GetName()) > 0);

			QuaternionTransform appliedTransform;
			bool transformSet = false;

			// ★ここが一番重要！ 上半身であり、かつ攻撃アニメが再生中なら、上半身を攻撃モーションにする！
			if (isUpperBody && upperAnimation_ && upperPlayMode_ != MotionPlayMode::Stop && !isUpperFinished_) {
				appliedTransform = GetTransformAnimation(*upperAnimation_, normName, upperAnimationTime_);
				transformSet = true;
			}
			// それ以外（下半身、または攻撃していない時の上半身）は、ベースの移動モーション（歩き・走り）にする！
			else {
				if (animationBlendState_.isBlending) {
					float t = std::clamp(animationBlendState_.currentTime / animationBlendState_.blendTime, 0.0f, 1.0f);
					QuaternionTransform fromTr = GetTransformAnimation(animationBlendState_.from, normName, animationBlendState_.fromTime + animationBlendState_.currentTime);
					QuaternionTransform toTr = GetTransformAnimation(animationBlendState_.to, normName, animationBlendState_.toTime + animationBlendState_.currentTime);

					appliedTransform.translate = Lerp(fromTr.translate, toTr.translate, t);
					appliedTransform.rotate = Slerp(fromTr.rotate, toTr.rotate, t);
					appliedTransform.scale = Lerp(fromTr.scale, toTr.scale, t);
					transformSet = true;
				}
				else if (animation_ && playMode_ != MotionPlayMode::Stop) {
					appliedTransform = GetTransformAnimation(*animation_, normName, animationTime_);
					transformSet = true;
				}
			}

			if (transformSet) {
				joint.SetTransform(appliedTransform);
			}
		}

		skeleton_->Update();
		if (skinCluster_) {
			skinCluster_->UpdateMatrixPalette(skeleton_->GetJoints());
		}
	}
	else if (node_ && animation_ && playMode_ != MotionPlayMode::Stop) {
		animation_->PlayerAnimation(animationTime_, *node_);
	}
}

// ============================================================
// 再生制御
// ============================================================
void MotionSystem::PlayOnce() {
	playMode_ = MotionPlayMode::Once;
	isFinished_ = false;
}

void MotionSystem::PlayLoop() {
	playMode_ = MotionPlayMode::Loop;
	isFinished_ = false;
}

void MotionSystem::Stop()
{
	if (playMode_ != MotionPlayMode::Stop) {
		prevPlayMode_ = playMode_;
		playMode_ = MotionPlayMode::Stop;
	}
}

void MotionSystem::Resume()
{
	if (playMode_ == MotionPlayMode::Stop) {
		playMode_ = prevPlayMode_;
		isFinished_ = false;
	}
}

// ============================================================
// ブレンド開始
// ============================================================
void MotionSystem::StartBlend(Motion& toAnimation, float blendDuration) {

	// ------------------------------------------------------------
	// ブレンド先のアニメーション確認
	// ------------------------------------------------------------
	for (Joint& joint : skeleton_->GetJoints()) {
		std::string name = NormalizeNodeName(joint.GetName());

		if (ignoreNodes.count(name)) {
			continue;
		}
		bool found = false;

		for (const auto& [nodeName, _] : toAnimation.animation_.nodeAnimations_) {
			if (NormalizeNodeName(nodeName) == name) {
				found = true;
				break;
			}
		}

		if (!found) {
			throw std::runtime_error("Motion: " + name + " Not found in Blend Destination");
		}
	}

	// ------------------------------------------------------------
	// ブレンド状態の初期化
	// ------------------------------------------------------------
	animationBlendState_.from = *animation_;
	animationBlendState_.fromTime = animationTime_;
	animationBlendState_.to = toAnimation;
	animationBlendState_.toTime = 0.0f;
	animationBlendState_.blendTime = blendDuration;
	animationBlendState_.currentTime = 0.0f;
	animationBlendState_.isBlending = true;
	animation_ = &animationBlendState_.to;
}

// ============================================================
// 上半身用アニメーションの再生制御
// ============================================================
void MotionSystem::PlayUpperAnimation(Motion* animation, MotionPlayMode mode){
	upperAnimation_ = animation;
	upperPlayMode_ = mode;
	upperAnimationTime_ = 0.0f;
	isUpperFinished_ = false;
	upperMotionSpeed_ = 1.0f;  // デフォルト速度にリセット（呼び出し側でSetUpperMotionSpeedを使って設定）
}

// ============================================================
// 上半身用アニメーションの停止
// ============================================================
void MotionSystem::StopUpperAnimation(){
	upperPlayMode_ = MotionPlayMode::Stop;
	upperAnimation_ = nullptr;
	isUpperFinished_ = false;
	// ★追加: マスクされたボーンの数をコンソールに出力する
	std::string msg = "[MotionSystem] 上半身マスクの登録数: " + std::to_string(upperBodyBoneMask_.size()) + "\n";
	OutputDebugStringA(msg.c_str());

	if (upperBodyBoneMask_.empty()) {
		OutputDebugStringA("[MotionSystem] 警告: 上半身マスクが空です！起点ボーンの名前が間違っている可能性があります。\n");
	}
}

// ============================================================
// 正規化されたノード名の取得
// ============================================================
std::string MotionSystem::GetNormalizedName(const std::string& name) {
	auto it = normalizedNameCache_.find(name);
	if (it != normalizedNameCache_.end()) return it->second;
	std::string normalized = NormalizeNodeName(name);
	normalizedNameCache_[name] = normalized;
	return normalized;
}

// ============================================================
// トランスフォームの取得
// ============================================================
QuaternionTransform MotionSystem::GetTransformAnimation(const Motion& anim, const std::string& nodeName, float time)
{
	QuaternionTransform qTransform{};
	const auto& animMap = anim.animation_.nodeAnimations_;

	auto it = std::find_if(animMap.begin(), animMap.end(),
		[&](const auto& pair) {
			return GetNormalizedName(pair.first) == GetNormalizedName(nodeName);
		});

	if (it != animMap.end()) {
		const auto& nodeAnim = it->second;
		qTransform.translate = const_cast<Motion&>(anim).CalculateValue(nodeAnim.translate.keyframes, time, nodeAnim.interpolationType);
		qTransform.rotate = const_cast<Motion&>(anim).CalculateValue(nodeAnim.rotate.keyframes, time, nodeAnim.interpolationType);
		qTransform.scale = const_cast<Motion&>(anim).CalculateValue(nodeAnim.scale.keyframes, time, nodeAnim.interpolationType);
	}
	else {
		qTransform.translate = { 0.0f, 0.0f, 0.0f };
		qTransform.rotate = { 0.0f, 0.0f, 0.0f, 1.0f };
		qTransform.scale = { 1.0f, 1.0f, 1.0f };
	}
	return qTransform;
}

// ============================================================
// モード・時間設定
// ============================================================
void MotionSystem::SetPlayMode(MotionPlayMode playMode)
{
	playMode_ = playMode;
	isFinished_ = false;

	if (!animationBlendState_.isBlending) {
		animationBlendState_.toTime = 0.0f;
		animationTime_ = 0.0f;
	}
}

void MotionSystem::SetAnimationTime(float time)
{
	if (!animation_) return;
	float duration = animation_->GetDuration();
	animationTime_ = std::clamp(time, 0.0f, duration);
}

// ============================================================
// ブレンド計算と適用
// ============================================================
void MotionSystem::BlendAndApplyAnimation(const Motion& from, const Motion& to, float t)
{
	float fromSampleTime = animationBlendState_.fromTime + animationBlendState_.currentTime;
	float toSampleTime = animationBlendState_.toTime + animationBlendState_.currentTime;

	for (Joint& joint : skeleton_->GetJoints()) {
		std::string name = GetNormalizedName(joint.GetName());

		if (ignoreNodes.count(name)) { continue; }

		QuaternionTransform fromTr = GetTransformAnimation(from, name, fromSampleTime);
		QuaternionTransform toTr = GetTransformAnimation(to, name, toSampleTime);
		QuaternionTransform blended;

		blended.translate = Lerp(fromTr.translate, toTr.translate, t);
		blended.rotate = Slerp(fromTr.rotate, toTr.rotate, t);
		blended.scale = Lerp(fromTr.scale, toTr.scale, t);

		joint.SetTransform(blended);
	}
}