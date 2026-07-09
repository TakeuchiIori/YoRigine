#include "MotionSystem.h"

#include "../../ModelUtils.h"
#include "Debugger/Logger.h"

#include <Windows.h>
#include <unordered_set>
#include <algorithm>
#include <stdexcept>

#include "Vector3.h"
#include "Quaternion.h"

namespace {
	using NodeAnimationLookup = std::unordered_map<std::string, const Motion::NodeAnimation*>;

	NodeAnimationLookup BuildNodeAnimationLookup(const Motion* motion)
	{
		NodeAnimationLookup lookup;
		if (!motion) {
			return lookup;
		}

		lookup.reserve(motion->animation_.nodeAnimations_.size());
		for (const auto& [nodeName, nodeAnimation] : motion->animation_.nodeAnimations_) {
			lookup[NormalizeNodeName(nodeName)] = &nodeAnimation;
		}
		return lookup;
	}

	const Motion::NodeAnimation* FindNodeAnimation(const NodeAnimationLookup& lookup, const std::string& normalizedName)
	{
		auto it = lookup.find(normalizedName);
		return it != lookup.end() ? it->second : nullptr;
	}

	QuaternionTransform SampleNodeAnimation(
		const Motion& motion,
		const Motion::NodeAnimation* nodeAnimation,
		float time,
		const QuaternionTransform& fallback)
	{
		if (!nodeAnimation) {
			return fallback;
		}

		QuaternionTransform transform{};
		transform.translate = const_cast<Motion&>(motion).CalculateValue(
			nodeAnimation->translate.keyframes, time, nodeAnimation->interpolationType);
		transform.rotate = const_cast<Motion&>(motion).CalculateValue(
			nodeAnimation->rotate.keyframes, time, nodeAnimation->interpolationType);
		transform.scale = const_cast<Motion&>(motion).CalculateValue(
			nodeAnimation->scale.keyframes, time, nodeAnimation->interpolationType);
		return transform;
	}
}

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
				StopUpperAnimation(0.15f); // ブレンドアウトで自然に下半身の動きに同期させる
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
	// 上半身ブレンドアウトの進行
	// ------------------------------------------------------------
	if (isUpperBlendingOut_) {
		upperBlendOutTimer_ += deltaTime;
		if (upperBlendOutTimer_ >= upperBlendOutDuration_) {
			isUpperBlendingOut_ = false;
			blendOutUpperAnimation_ = nullptr;
		}
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
		const NodeAnimationLookup baseLookup = BuildNodeAnimationLookup(animation_);
		const NodeAnimationLookup upperLookup = BuildNodeAnimationLookup(
			upperAnimation_ && upperPlayMode_ != MotionPlayMode::Stop && !isUpperFinished_ ? upperAnimation_ : nullptr);
		const NodeAnimationLookup blendOutUpperLookup = BuildNodeAnimationLookup(
			isUpperBlendingOut_ ? blendOutUpperAnimation_ : nullptr);
		const NodeAnimationLookup blendFromLookup = animationBlendState_.isBlending
			? BuildNodeAnimationLookup(&animationBlendState_.from)
			: NodeAnimationLookup{};
		const NodeAnimationLookup blendToLookup = animationBlendState_.isBlending
			? BuildNodeAnimationLookup(&animationBlendState_.to)
			: NodeAnimationLookup{};

		for (Joint& joint : skeleton_->GetJoints()) {
			std::string normName = GetNormalizedName(joint.GetName());

			// マスクに登録されているか（上半身かどうか）の判定
			bool isUpperBody = (upperBodyBoneMask_.count(joint.GetName()) > 0);

			QuaternionTransform appliedTransform;
			bool transformSet = false;
			const Motion::NodeAnimation* upperNode = FindNodeAnimation(upperLookup, normName);

			// ★ここが一番重要！ 上半身であり、かつ攻撃アニメが再生中なら、上半身を攻撃モーションにする！
			if (isUpperBody && upperAnimation_ && upperPlayMode_ != MotionPlayMode::Stop && !isUpperFinished_ && upperNode) {
				appliedTransform = SampleNodeAnimation(*upperAnimation_, upperNode, upperAnimationTime_, joint.GetTransform());
				transformSet = true;
			}
			// ★ブレンドアウト中：上半身アニメーションからベースアニメーションへ徐々に遷移
			else if (isUpperBody && isUpperBlendingOut_ && blendOutUpperAnimation_) {
				float t = std::clamp(upperBlendOutTimer_ / upperBlendOutDuration_, 0.0f, 1.0f);
				QuaternionTransform upperTr = SampleNodeAnimation(
					*blendOutUpperAnimation_,
					FindNodeAnimation(blendOutUpperLookup, normName),
					blendOutUpperAnimTime_,
					joint.GetTransform());

				// ベースレイヤーのトランスフォームを取得
				QuaternionTransform baseTr = joint.GetTransform();

				if (animationBlendState_.isBlending) {
					float bt = std::clamp(animationBlendState_.currentTime / animationBlendState_.blendTime, 0.0f, 1.0f);
					QuaternionTransform fromTr = SampleNodeAnimation(
						animationBlendState_.from,
						FindNodeAnimation(blendFromLookup, normName),
						animationBlendState_.fromTime + animationBlendState_.currentTime,
						joint.GetTransform());
					QuaternionTransform toTr = SampleNodeAnimation(
						animationBlendState_.to,
						FindNodeAnimation(blendToLookup, normName),
						animationBlendState_.toTime + animationBlendState_.currentTime,
						joint.GetTransform());
					baseTr.translate = Lerp(fromTr.translate, toTr.translate, bt);
					baseTr.rotate = Slerp(fromTr.rotate, toTr.rotate, bt);
					baseTr.scale = Lerp(fromTr.scale, toTr.scale, bt);
				}
				else if (animation_ && playMode_ != MotionPlayMode::Stop) {
					baseTr = SampleNodeAnimation(
						*animation_,
						FindNodeAnimation(baseLookup, normName),
						animationTime_,
						joint.GetTransform());
				}

				// 攻撃ポーズからベースポーズへ補間（t: 0→1）
				appliedTransform.translate = Lerp(upperTr.translate, baseTr.translate, t);
				appliedTransform.rotate = Slerp(upperTr.rotate, baseTr.rotate, t);
				appliedTransform.scale = Lerp(upperTr.scale, baseTr.scale, t);
				transformSet = true;
			}
			// それ以外（下半身、または攻撃していない時の上半身）は、ベースの移動モーション（歩き・走り）にする！
			else {
				if (animationBlendState_.isBlending) {
					float t = std::clamp(animationBlendState_.currentTime / animationBlendState_.blendTime, 0.0f, 1.0f);
					const Motion::NodeAnimation* fromNode = FindNodeAnimation(blendFromLookup, normName);
					const Motion::NodeAnimation* toNode = FindNodeAnimation(blendToLookup, normName);
					if (!fromNode && !toNode) {
						continue;
					}
					QuaternionTransform fromTr = SampleNodeAnimation(
						animationBlendState_.from,
						fromNode,
						animationBlendState_.fromTime + animationBlendState_.currentTime,
						joint.GetTransform());
					QuaternionTransform toTr = SampleNodeAnimation(
						animationBlendState_.to,
						toNode,
						animationBlendState_.toTime + animationBlendState_.currentTime,
						joint.GetTransform());

					appliedTransform.translate = Lerp(fromTr.translate, toTr.translate, t);
					appliedTransform.rotate = Slerp(fromTr.rotate, toTr.rotate, t);
					appliedTransform.scale = Lerp(fromTr.scale, toTr.scale, t);
					transformSet = true;
				}
				else if (animation_ && playMode_ != MotionPlayMode::Stop) {
					const Motion::NodeAnimation* baseNode = FindNodeAnimation(baseLookup, normName);
					if (!baseNode) {
						continue;
					}
					appliedTransform = SampleNodeAnimation(*animation_, baseNode, animationTime_, joint.GetTransform());
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
void MotionSystem::StopUpperAnimation(float blendOutDuration){
	// 既にブレンドアウト中の場合は何もしない（進行中のブレンドアウトを継続）
	if (isUpperBlendingOut_) return;

	if (blendOutDuration > 0.0f && upperAnimation_) {
		// ブレンドアウト開始：上半身アニメーションからベースアニメーションへ徐々に遷移
		isUpperBlendingOut_ = true;
		upperBlendOutTimer_ = 0.0f;
		upperBlendOutDuration_ = blendOutDuration;
		blendOutUpperAnimation_ = upperAnimation_;
		blendOutUpperAnimTime_ = upperAnimationTime_;
	}

	// 通常の上半身アニメーションは停止
	upperPlayMode_ = MotionPlayMode::Stop;
	upperAnimation_ = nullptr;
	isUpperFinished_ = false;
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

bool MotionSystem::HasTransformAnimation(const Motion& anim, const std::string& nodeName)
{
	const auto& animMap = anim.animation_.nodeAnimations_;
	return std::any_of(animMap.begin(), animMap.end(),
		[&](const auto& pair) {
			return GetNormalizedName(pair.first) == GetNormalizedName(nodeName);
		});
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
	const NodeAnimationLookup fromLookup = BuildNodeAnimationLookup(&from);
	const NodeAnimationLookup toLookup = BuildNodeAnimationLookup(&to);

	for (Joint& joint : skeleton_->GetJoints()) {
		std::string name = GetNormalizedName(joint.GetName());

		const Motion::NodeAnimation* fromNode = FindNodeAnimation(fromLookup, name);
		const Motion::NodeAnimation* toNode = FindNodeAnimation(toLookup, name);
		if (!fromNode && !toNode) {
			continue;
		}

		QuaternionTransform fromTr = SampleNodeAnimation(from, fromNode, fromSampleTime, joint.GetTransform());
		QuaternionTransform toTr = SampleNodeAnimation(to, toNode, toSampleTime, joint.GetTransform());
		QuaternionTransform blended;

		blended.translate = Lerp(fromTr.translate, toTr.translate, t);
		blended.rotate = Slerp(fromTr.rotate, toTr.rotate, t);
		blended.scale = Lerp(fromTr.scale, toTr.scale, t);

		joint.SetTransform(blended);
	}
}
