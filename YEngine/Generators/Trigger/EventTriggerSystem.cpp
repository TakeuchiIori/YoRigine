#include "EventTriggerSystem.h"

#include "Actions/RuleTriggerAction.h"
#include "EventTriggerLoader.h"

#include <utility>

void EventTriggerSystem::Clear() {
	openGateActions_.clear();
	triggers_.clear();
}

bool EventTriggerSystem::Load(const std::string& filePath,
                              Camera* camera,
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
	for (auto& trigger : triggers_) {
		if (trigger && trigger->GetAction()) {
			trigger->GetAction()->NotifyEnemyDefeated(group);
		}
	}
}
