#include "WaypointAction.h"

#include "../EventTrigger.h"
#include "../WaypointManager.h"

WaypointAction::WaypointAction(std::string beaconEffect, std::string requiredGroup,
                               int requiredCount, std::string nextWaypoint, bool startActive)
	: beaconEffect_(std::move(beaconEffect))
	, requiredGroup_(std::move(requiredGroup))
	, requiredCount_(requiredCount > 0 ? requiredCount : 1)
	, nextWaypoint_(std::move(nextWaypoint))
	, startActive_(startActive) {}

WaypointAction::~WaypointAction() {
	WaypointManager::GetInstance()->Unregister(this);
}

void WaypointAction::OnAttach(EventTrigger* owner) {
	owner_ = owner;
}

void WaypointAction::Update(float /*deltaTime*/) {
	// owner の名前が付いてから登録する（SetName / SetAction の呼び順に依存しないよう遅延登録）。
	if (!registered_ && owner_ && !owner_->GetName().empty()) {
		WaypointManager::GetInstance()->Register(owner_->GetName(), this);
		registered_ = true;

		// 最初の目的地は登録直後に自動でアクティブ化する（一度だけ）。
		if (startActive_ && !startTriggered_) {
			startTriggered_ = true;
			WaypointManager::GetInstance()->Activate(owner_->GetName());
		}
	}
}

void WaypointAction::NotifyEnemyDefeated(const std::string& group) {
	if (!active_) return;                                        // 現在の目的地のみカウント
	if (!requiredGroup_.empty() && group != requiredGroup_) return; // 空 = ワイルドカード

	if (++currentCount_ >= requiredCount_) {
		// クリア → 次のウェイポイントへ（Manager がビーコン停止＋次のアクティブ化を行う）。
		WaypointManager::GetInstance()->Activate(nextWaypoint_);
	}
}

void WaypointAction::SetActive(bool a) {
	active_ = a;
	if (!a) currentCount_ = 0; // 非アクティブ化でカウントリセット
}

Vector3 WaypointAction::GetWorldPosition() const {
	return owner_ ? owner_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f };
}

nlohmann::json WaypointAction::SerializeToJson() const {
	return nlohmann::json{
		{"type",          GetTypeName()},
		{"beaconEffect",  beaconEffect_},
		{"beaconScale",   beaconScale_},
		{"requiredGroup", requiredGroup_},
		{"requiredCount", requiredCount_},
		{"nextWaypoint",  nextWaypoint_},
		{"startActive",   startActive_},
	};
}
