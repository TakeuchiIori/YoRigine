#include "TriggerActionFactory.h"

#include "Actions/OpenGateAction.h"
#include "Actions/RuleTriggerAction.h"
#include "Actions/WaypointAction.h"
#include "Debugger/Logger.h"

namespace {

	Vector3 ReadVec3(const nlohmann::json& json, const char* key, const Vector3& fallback) {
		if (!json.contains(key) || !json[key].is_array() || json[key].size() < 3) {
			return fallback;
		}
		return Vector3{
			json[key][0].get<float>(),
			json[key][1].get<float>(),
			json[key][2].get<float>()
		};
	}

} // namespace

std::unique_ptr<TriggerAction> TriggerActionFactory::Create(const nlohmann::json& actionJson) {
	if (!actionJson.contains("type")) {
		Logger("[TriggerActionFactory] action.type が無いためスキップ\n");
		return nullptr;
	}

	const std::string type = actionJson.value("type", std::string{});

	if (type == "OpenGate") {
		const std::string targetName    = actionJson.value("targetName", std::string{});
		const std::string requiredGroup = actionJson.value("requiredGroup", std::string{});
		const int         requiredCount = actionJson.value("requiredCount", 1);

		// requiredGroup は空文字でも OK (= ワイルドカード)。targetName だけ必須。
		if (targetName.empty()) {
			Logger("[TriggerActionFactory] OpenGate: targetName が空のためスキップ\n");
			return nullptr;
		}

		auto action = std::make_unique<OpenGateAction>(targetName, requiredGroup, requiredCount);

		action->SetOpenOffsetPosition(   ReadVec3(actionJson, "openOffsetPosition",    { 0.0f, 5.0f, 0.0f }));
		action->SetOpenOffsetRotationDeg(ReadVec3(actionJson, "openOffsetRotationDeg", { 0.0f, 0.0f, 0.0f }));
		action->SetOpenOffsetScale(      ReadVec3(actionJson, "openOffsetScale",       { 0.0f, 0.0f, 0.0f }));
		action->SetOpenDuration(actionJson.value("openDuration", 1.0f));

		// 閉位置 (永続化) があれば読み込む
		if (actionJson.value("closedCaptured", false)) {
			action->SetClosedPose(
				ReadVec3(actionJson, "closedPosition", { 0.0f, 0.0f, 0.0f }),
				ReadVec3(actionJson, "closedRotation", { 0.0f, 0.0f, 0.0f }),
				ReadVec3(actionJson, "closedScale",    { 1.0f, 1.0f, 1.0f }));
		}

		return action;
	}

	if (type == "Rule") {
		auto action = std::make_unique<RuleTriggerAction>();

		if (actionJson.contains("conditions") && actionJson["conditions"].is_array()) {
			for (const auto& c : actionJson["conditions"]) {
				RuleTriggerAction::Condition condition;
				condition.type = c.value("type", std::string("EnemyDefeatCount"));
				condition.group = c.value("group", std::string{});
				condition.name = c.value("name", std::string{});
				condition.requiredCount = c.value("count", 1);
				if (condition.requiredCount <= 0) condition.requiredCount = 1;
				action->AddCondition(condition);
			}
		}

		if (actionJson.contains("actions") && actionJson["actions"].is_array()) {
			for (const auto& a : actionJson["actions"]) {
				RuleTriggerAction::Command command;
				command.type = a.value("type", std::string("OpenGate"));
				command.targetName = a.value("targetName", std::string{});
				command.openOffsetPosition = ReadVec3(a, "openOffsetPosition", { 0.0f, 5.0f, 0.0f });
				command.openOffsetRotationDeg = ReadVec3(a, "openOffsetRotationDeg", { 0.0f, 0.0f, 0.0f });
				command.openOffsetScale = ReadVec3(a, "openOffsetScale", { 0.0f, 0.0f, 0.0f });
				command.openDuration = a.value("openDuration", 1.0f);
				if (command.openDuration <= 0.0f) command.openDuration = 0.0001f;
				action->AddCommand(command);
			}
		}

		return action;
	}

	if (type == "Waypoint") {
		const std::string beaconEffect  = actionJson.value("beaconEffect",  std::string{});
		const std::string requiredGroup = actionJson.value("requiredGroup", std::string{});
		const int         requiredCount = actionJson.value("requiredCount", 1);
		const std::string nextWaypoint  = actionJson.value("nextWaypoint",  std::string{});
		const bool        startActive   = actionJson.value("startActive",   false);

		auto action = std::make_unique<WaypointAction>(
			beaconEffect, requiredGroup, requiredCount, nextWaypoint, startActive);
		action->SetBeaconScale(actionJson.value("beaconScale", 1.0f));
		action->SetMissionTitle(actionJson.value("missionTitle", std::string{}));
		return action;
	}

	Logger("[TriggerActionFactory] 未知の type: " + type + "\n");
	return nullptr;
}
