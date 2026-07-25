#include "EventTriggerSystem.h"

#include "Actions/RuleTriggerAction.h"
#include "Actions/WaypointAction.h"
#include "EventTriggerLoader.h"

#include <utility>

void EventTriggerSystem::Clear() {
	openGateActions_.clear();
	triggers_.clear();
}

bool EventTriggerSystem::Load(const std::string& filePath,
                              YoRigine::Camera* camera,
                              std::function<void()> onAnyGateOpened) {
	Clear();
	bool loaded = EventTriggerLoader::Load(
		filePath,
		camera,
		onAnyGateOpened,
		triggers_,
		openGateActions_);

	for (auto& trigger : triggers_) {
		if (!trigger) continue;
		if (auto* rule = dynamic_cast<RuleTriggerAction*>(trigger->GetAction())) {
			rule->SetOnGateOpened(onAnyGateOpened);
		}
	}
	return loaded;
}

bool EventTriggerSystem::Save(const std::string& filePath) const {
	return EventTriggerLoader::Save(filePath, triggers_);
}

void EventTriggerSystem::Update() {
	for (auto& trigger : triggers_) {
		if (trigger) trigger->Update();
	}
}

void EventTriggerSystem::DrawCollision() {
	for (auto& trigger : triggers_) {
		if (trigger) trigger->DrawCollision();
	}
}

void EventTriggerSystem::NotifyEnemyDefeated(const std::string& group) {
	// 擊破通知の配信中に WaypointAction がミッションをクリアし、
	// 次のウェイポイントを有効化する。全トリガーへそのまま順番に
	// 通知すると、同じ1体の擊破が新しいミッションにも重複して
	// カウントされる。通知開始時点で有効だったものを固定する。
	WaypointAction* activeWaypoint = nullptr;
	for (auto& trigger : triggers_) {
		if (!trigger) continue;
		auto* waypoint = dynamic_cast<WaypointAction*>(trigger->GetAction());
		if (waypoint && waypoint->IsActive()) {
			activeWaypoint = waypoint;
			break;
		}
	}

	// ウェイポイント以外のギミックには従来通り配信する。
	for (auto& trigger : triggers_) {
		if (!trigger || !trigger->GetAction()) continue;
		if (dynamic_cast<WaypointAction*>(trigger->GetAction())) continue;
		trigger->GetAction()->NotifyEnemyDefeated(group);
	}

	// 現在のミッションだけを1回進める。クリア時は WaypointAction が
	// nextWaypoint を有効化し、次側の requiredCount が新しい必要数になる。
	if (activeWaypoint) {
		activeWaypoint->NotifyEnemyDefeated(group);
	}
}
