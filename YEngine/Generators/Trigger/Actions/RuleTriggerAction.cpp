#include "RuleTriggerAction.h"

#include "OpenGateAction.h"
#include "../WaypointManager.h"

bool RuleTriggerAction::Condition::IsSatisfied() const {
	return currentCount >= requiredCount;
}

nlohmann::json RuleTriggerAction::Condition::SerializeToJson() const {
	nlohmann::json j{
		{"type", type},
		{"count", requiredCount},
	};
	if (!group.empty()) j["group"] = group;
	if (!name.empty()) j["name"] = name;
	return j;
}

nlohmann::json RuleTriggerAction::Command::SerializeToJson() const {
	nlohmann::json j{
		{"type", type},
	};
	if (!targetName.empty()) j["targetName"] = targetName;

	if (type == "OpenGate") {
		j["openOffsetPosition"] = { openOffsetPosition.x, openOffsetPosition.y, openOffsetPosition.z };
		j["openOffsetRotationDeg"] = { openOffsetRotationDeg.x, openOffsetRotationDeg.y, openOffsetRotationDeg.z };
		j["openOffsetScale"] = { openOffsetScale.x, openOffsetScale.y, openOffsetScale.z };
		j["openDuration"] = openDuration;
	}
	return j;
}

void RuleTriggerAction::Update(float deltaTime) {
	for (auto& gate : runningGates_) {
		if (gate) gate->Update(deltaTime);
	}
}

void RuleTriggerAction::NotifyEnemyDefeated(const std::string& group) {
	if (fired_) return;

	bool touched = false;
	for (auto& condition : conditions_) {
		if (condition.type != "EnemyDefeatCount") continue;
		if (!condition.group.empty() && condition.group != group) continue;
		++condition.currentCount;
		touched = true;
	}

	if (touched) {
		TryFire();
	}
}

nlohmann::json RuleTriggerAction::SerializeToJson() const {
	nlohmann::json j;
	j["type"] = GetTypeName();
	j["conditions"] = nlohmann::json::array();
	j["actions"] = nlohmann::json::array();

	for (const auto& condition : conditions_) {
		j["conditions"].push_back(condition.SerializeToJson());
	}
	for (const auto& command : commands_) {
		j["actions"].push_back(command.SerializeToJson());
	}
	return j;
}

void RuleTriggerAction::ResetRuntime() {
	fired_ = false;
	runningGates_.clear();
	for (auto& condition : conditions_) {
		condition.currentCount = 0;
	}
}

void RuleTriggerAction::TryFire() {
	if (fired_ || conditions_.empty()) return;
	for (const auto& condition : conditions_) {
		if (!condition.IsSatisfied()) return;
	}

	fired_ = true;
	for (const auto& command : commands_) {
		ExecuteCommand(command);
	}
}

void RuleTriggerAction::ExecuteCommand(const Command& command) {
	if (command.type == "OpenGate") {
		auto gate = std::make_unique<OpenGateAction>(command.targetName, std::string{}, 1);
		gate->SetOpenOffsetPosition(command.openOffsetPosition);
		gate->SetOpenOffsetRotationDeg(command.openOffsetRotationDeg);
		gate->SetOpenOffsetScale(command.openOffsetScale);
		gate->SetOpenDuration(command.openDuration);
		gate->SetOnGateOpened(onGateOpened_);
		gate->TriggerPreview();
		runningGates_.push_back(std::move(gate));
		return;
	}

	if (command.type == "ActivateWaypoint") {
		WaypointManager::GetInstance()->Activate(command.targetName);
		return;
	}

	if (command.type == "ClearWaypoint") {
		WaypointManager::GetInstance()->Activate("");
		return;
	}
}
