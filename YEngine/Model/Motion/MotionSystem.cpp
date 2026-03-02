#include "MotionSystem.h"

// Engine
#include "../ModelUtils.h"
#include "Debugger/Logger.h"

// C++
#include <Windows.h>
#include <unordered_set>

// MAth
#include "Vector3.h"
#include "Quaternion.h"
void MotionSystem::Initialize(Motion& Motion, Skeleton& skeleton, SkinCluster& skinCluster, Node* node)
{
	animation_ = &Motion;
	skeleton_ = &skeleton;
	skinCluster_ = &skinCluster;
	node_ = node;
	animationTime_ = 0.0f;
}

void MotionSystem::Initialize(Motion& Motion, Node* rootNode)
{
	animation_ = &Motion;
	node_ = rootNode;
	animationTime_ = 0.0f;
}

void MotionSystem::Update(float deltaTime)
{
	if (!animation_ || playMode_ == MotionPlayMode::Stop || isFinished_) return;

	// ブレンド中の処理
	if (animationBlendState_.isBlending) {
		animationBlendState_.currentTime += deltaTime;

		if (animationBlendState_.currentTime >= animationBlendState_.blendTime) {
			animationBlendState_.isBlending = false;
			animationTime_ = animationBlendState_.toTime + animationBlendState_.currentTime;
		}
		return;
	}

	bool wasFinished = isFinished_;
	// 通常のアニメーション再生処理
	animationTime_ += deltaTime * GetEffectiveSpeed();

	float duration = animation_->GetDuration();
	if (animationTime_ >= duration) {
		if (playMode_ == MotionPlayMode::Loop) {
			animationTime_ = 0.0f;
		} else {
			animationTime_ = duration;
			isFinished_ = true;
		}
	}

	// 終了した瞬間コールバックする
	if (!wasFinished && isFinished_) {
		if (onMationFinished_) {
			onMationFinished_();
		}
	}


}


// アニメーション適用
void MotionSystem::Apply()
{
	if (!animation_ || playMode_ == MotionPlayMode::Stop) return;

	if (animationBlendState_.isBlending && skeleton_) {
		float t = animationBlendState_.currentTime / animationBlendState_.blendTime;
		t = std::clamp(t, 0.0f, 1.0f);
		BlendAndApplyAnimation(animationBlendState_.from, animationBlendState_.to, t);

		skeleton_->Update();
		if (skinCluster_) {
			skinCluster_->UpdateMatrixPalette(skeleton_->GetJoints());
		}
	} else if (skeleton_) {
		animation_->ApplyAnimation(skeleton_->GetJoints(), animationTime_);
		skeleton_->Update();
		if (skinCluster_) {
			skinCluster_->UpdateMatrixPalette(skeleton_->GetJoints());
		}
	} else if (node_) {
		animation_->PlayerAnimation(animationTime_, *node_);
	}
}


void MotionSystem::PlayOnce() {
	//animationTime_ = 0.0f;
	playMode_ = MotionPlayMode::Once;
	isFinished_ = false;
}

void MotionSystem::PlayLoop() {
	//animationTime_ = 0.0f;
	playMode_ = MotionPlayMode::Loop;
	isFinished_ = false;
}

void MotionSystem::Stop()
{
	if (playMode_ != MotionPlayMode::Stop) {
		// 現在の状態を保存
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

void MotionSystem::StartBlend(Motion& toAnimation, float blendDuration) {

	for (Joint& joint : skeleton_->GetJoints()) {
		std::string name = NormalizeNodeName(joint.GetName());

		if (ignoreNodes.count(name)) {
			continue; // 無視
		}
		bool found = false;

		for (const auto& [nodeName, _] : toAnimation.animation_.nodeAnimations_) {
			if (NormalizeNodeName(nodeName) == name) {
				found = true;
				break;
			}
		}

		if (!found) {
			throw std::runtime_error("Motion" + name + "Not Blend Destination"); // ブレンド先が見つからない
		}
	}

	/// アニメーションのブレンドの初期化
	animationBlendState_.from = *animation_;
	animationBlendState_.fromTime = animationTime_;		// 現在の再生位置を保存
	animationBlendState_.to = toAnimation;
	animationBlendState_.toTime = 0.0f;					// 必要なら to 側も途中から再生可
	animationBlendState_.blendTime = blendDuration;
	animationBlendState_.currentTime = 0.0f;
	animationBlendState_.isBlending = true;
	animation_ = &animationBlendState_.to;				// 今後は to を再生
}
std::string MotionSystem::GetNormalizedName(const std::string& name) {
	auto it = normalizedNameCache_.find(name);
	if (it != normalizedNameCache_.end()) return it->second;
	std::string normalized = NormalizeNodeName(name);
	normalizedNameCache_[name] = normalized;
	return normalized;
}

QuaternionTransform MotionSystem::GetTransformAnimation(const Motion& anim, const std::string& nodeName, float time)
{
	QuaternionTransform qTransform{};
	const auto& animMap = anim.animation_.nodeAnimations_;

	// 読み込み先と比較してノード名探し
	auto it = std::find_if(animMap.begin(), animMap.end(),
		[&](const auto& pair) {
			return GetNormalizedName(pair.first) == GetNormalizedName(nodeName);
		});

	if (it != animMap.end()) {
		const auto& nodeAnim = it->second;
		qTransform.translate = const_cast<Motion&>(anim).CalculateValue(nodeAnim.translate.keyframes, time, nodeAnim.interpolationType);
		qTransform.rotate = const_cast<Motion&>(anim).CalculateValue(nodeAnim.rotate.keyframes, time, nodeAnim.interpolationType);
		qTransform.scale = const_cast<Motion&>(anim).CalculateValue(nodeAnim.scale.keyframes, time, nodeAnim.interpolationType);
	} else {
		qTransform.translate = { 0.0f, 0.0f, 0.0f };
		qTransform.rotate = { 0.0f, 0.0f, 0.0f, 1.0f };
		qTransform.scale = { 1.0f, 1.0f, 1.0f };
	}
	return qTransform;
}

void MotionSystem::SetPlayMode(MotionPlayMode playMode)
{
	playMode_ = playMode;
	isFinished_ = false;

	if (!animationBlendState_.isBlending) {
		//animationBlendState_.fromTime = animationTime_;
		animationBlendState_.toTime = 0.0f;
		animationTime_ = 0.0f;
	}
}

void MotionSystem::BlendAndApplyAnimation(const Motion& from, const Motion& to, float t)
{
	float fromSampleTime = animationBlendState_.fromTime + animationBlendState_.currentTime;
	float toSampleTime = animationBlendState_.toTime + animationBlendState_.currentTime;

	for (Joint& joint : skeleton_->GetJoints()) {
		std::string name = GetNormalizedName(joint.GetName());

		// ノード名を除く
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


