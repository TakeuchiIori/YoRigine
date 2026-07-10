#include "OpenGateAction.h"

#include "Object3D/ObjectManager.h"
#include "Collision/Core/BaseCollider.h"
#include "MathFunc.h"
#include "Debugger/Logger.h"

namespace {
	inline float Clamp01(float v) { return (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v); }
}

OpenGateAction::OpenGateAction(std::string targetName, std::string requiredGroup, int requiredCount)
	: targetName_(std::move(targetName))
	, requiredGroup_(std::move(requiredGroup))
	, requiredCount_(requiredCount > 0 ? requiredCount : 1) {}

void OpenGateAction::NotifyEnemyDefeated(const std::string& group) {
	if (phase_ != Phase::Waiting) return;
	// requiredGroup_ が空文字ならワイルドカード扱い (どの敵を倒してもカウント)。
	if (!requiredGroup_.empty() && group != requiredGroup_) return;

	++currentCount_;
	if (currentCount_ >= requiredCount_) {
		defeatSignal_ = true;
	}
}

void OpenGateAction::Update(float deltaTime) {
	if (phase_ == Phase::Waiting && defeatSignal_) {
		BeginOpening();
	}
	if (phase_ == Phase::Opening) {
		TickOpening(deltaTime);
	}
}

void OpenGateAction::CaptureClosedPoseFromTarget() {
	auto* om = ObjectManager::GetInstance();
	auto* target = om ? om->GetObjectByName(targetName_) : nullptr;
	if (!target) return;
	closedPosition_ = target->position;
	closedRotation_ = target->rotation;
	closedScale_    = target->scale;
	closedCaptured_ = true;
	Logger("[OpenGate] 閉位置を捕捉: target=\"" + targetName_ + "\"\n");
}

void OpenGateAction::SetClosedPose(const Vector3& pos, const Vector3& rot, const Vector3& scale) {
	closedPosition_ = pos;
	closedRotation_ = rot;
	closedScale_    = scale;
	closedCaptured_ = true;
}

void OpenGateAction::RestoreClosed() {
	auto* om = ObjectManager::GetInstance();
	auto* target = om ? om->GetObjectByName(targetName_) : nullptr;
	if (!target) return;

	// closed pose 未捕捉なら、初回として現在の target 位置を「閉」と見なす
	if (!closedCaptured_) {
		CaptureClosedPoseFromTarget();
	}

	target->position = closedPosition_;
	target->rotation = closedRotation_;
	target->scale    = closedScale_;
	target->colliderEnabled = true;
	if (target->collider) {
		target->collider->SetCollisionEnabled(true);
	}
	om->UpdateObjectTransform(*target);
	if (target->collider) target->collider->Update();

	phase_         = Phase::Waiting;
	currentCount_  = 0;
	defeatSignal_  = false;
	elapsed_       = 0.0f;
	cachedTargetId_ = target->id;
}

void OpenGateAction::BeginOpening() {
	auto* om = ObjectManager::GetInstance();
	auto* target = om ? om->GetObjectByName(targetName_) : nullptr;
	if (!target) {
		Logger("[OpenGate] ターゲット未解決: nameTag=\"" + targetName_ + "\"\n");
		defeatSignal_ = false;
		return;
	}

	// closed pose 未捕捉ならここで捕捉する (RestoreClosed が呼ばれてないケースの保険)
	if (!closedCaptured_) {
		CaptureClosedPoseFromTarget();
	}

	cachedTargetId_   = target->id;
	elapsed_          = 0.0f;
	phase_            = Phase::Opening;

	Logger("[OpenGate] Opening start: target=\"" + targetName_ +
		"\" duration=" + std::to_string(openDuration_) + "s\n");
}

void OpenGateAction::TickOpening(float dt) {
	auto* om = ObjectManager::GetInstance();
	if (!om) return;
	auto* target = om->GetObjectById(cachedTargetId_);
	if (!target) {
		phase_ = Phase::Opened;
		return;
	}

	elapsed_ += dt;
	const float t = Clamp01(elapsed_ / openDuration_);

	const Vector3 offsetRotRad = {
		DegToRad(openOffsetRotationDeg_.x),
		DegToRad(openOffsetRotationDeg_.y),
		DegToRad(openOffsetRotationDeg_.z),
	};

	target->position = closedPosition_ + openOffsetPosition_ * t;
	target->rotation = closedRotation_ + offsetRotRad         * t;
	target->scale    = closedScale_    + openOffsetScale_     * t;
	om->UpdateObjectTransform(*target);
	if (target->collider) {
		target->collider->Update();
	}

	if (t >= 1.0f) {
		// 完了: 物理的に通り抜けられるよう collider を無効化、後処理を発火。
		target->colliderEnabled = false;
		if (target->collider) {
			target->collider->SetCollisionEnabled(false);
		}
		phase_ = Phase::Opened;
		Logger("[OpenGate] Opened: target=\"" + targetName_ + "\"\n");
		if (onGateOpened_) onGateOpened_();
	}
}

void OpenGateAction::Reset() {
	// RestoreClosed と同じ意味: 閉位置に戻して Waiting からやり直し
	RestoreClosed();
}

void OpenGateAction::TriggerPreview() {
	// 閉位置にリセットしてから即発火
	RestoreClosed();
	defeatSignal_ = true;
	currentCount_ = requiredCount_;
	Logger("[OpenGate] Preview triggered: target=\"" + targetName_ + "\"\n");
}

nlohmann::json OpenGateAction::SerializeToJson() const {
	return nlohmann::json{
		{"type",          GetTypeName()},
		{"targetName",    targetName_},
		{"requiredGroup", requiredGroup_},
		{"requiredCount", requiredCount_},
		{"openOffsetPosition",    { openOffsetPosition_.x,    openOffsetPosition_.y,    openOffsetPosition_.z }},
		{"openOffsetRotationDeg", { openOffsetRotationDeg_.x, openOffsetRotationDeg_.y, openOffsetRotationDeg_.z }},
		{"openOffsetScale",       { openOffsetScale_.x,       openOffsetScale_.y,       openOffsetScale_.z }},
		{"openDuration",          openDuration_},
		// 閉位置 (永続化することで、ゲーム再起動時に target が誤った状態でも復元できる)
		{"closedCaptured",  closedCaptured_},
		{"closedPosition",  { closedPosition_.x, closedPosition_.y, closedPosition_.z }},
		{"closedRotation",  { closedRotation_.x, closedRotation_.y, closedRotation_.z }},
		{"closedScale",     { closedScale_.x,    closedScale_.y,    closedScale_.z }},
	};
}
