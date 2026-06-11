#include "OpenGateAction.h"

#include "Object3D/ObjectManager.h"
#include "Object3D/Object3d.h"
#include "Collision/Core/BaseCollider.h"
#include "Motion/Core/MotionSystem.h"

OpenGateAction::OpenGateAction(std::string targetName, std::string requiredGroup)
	: targetName_(std::move(targetName))
	, requiredGroup_(std::move(requiredGroup)) {}

void OpenGateAction::SetOpenAnimation(const std::string& file, const std::string& clipName) {
	openAnimFile_ = file;
	openAnimClip_ = clipName;
}

void OpenGateAction::NotifyEnemyDefeated(const std::string& group) {
	if (phase_ != Phase::Waiting) return;
	if (group == requiredGroup_) {
		defeatSignal_ = true;
	}
}

void OpenGateAction::Update(float /*deltaTime*/) {
	if (phase_ == Phase::Waiting && defeatSignal_) {
		TryOpen();
	}
}

void OpenGateAction::TryOpen() {
	auto* om = ObjectManager::GetInstance();
	auto* target = om ? om->GetObjectByName(targetName_) : nullptr;
	if (!target) {
		// ターゲット未配置/未命名。次フレームも再試行できるようフラグは残し、Phase は Waiting のまま。
		return;
	}

	// 1. 衝突を無効化 (ブロッカーとして機能していた場合に通り抜け可能になる)
	target->colliderEnabled = false;
	if (target->collider) {
		target->collider->SetCollisionEnabled(false);
	}

	// 2. 開放アニメーションへ切り替え (設定されていれば)
	if (!openAnimFile_.empty() && target->object) {
		target->object->SetChangeMotion(openAnimFile_, MotionPlayMode::Once, openAnimClip_);
	}

	phase_ = Phase::Opening;

	// 3. NavGrid 再ベイクなどの後処理を依頼
	if (onGateOpened_) {
		onGateOpened_();
	}

	// MVP では Opening の継続時間を持たず、即 Opened へ。
	// アニメ完了を待ちたい場合は将来的に Object3d 側に終了通知 API が欲しい。
	phase_ = Phase::Opened;
}
