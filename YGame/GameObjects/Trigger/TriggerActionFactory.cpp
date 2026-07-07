#include "TriggerActionFactory.h"

#include "Actions/OpenGateAction.h"
#include "Actions/WaypointAction.h"
#include "Debugger/Logger.h"

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

		auto readVec3 = [&](const char* key, const Vector3& fallback) -> Vector3 {
			if (!actionJson.contains(key) || !actionJson[key].is_array() || actionJson[key].size() < 3) {
				return fallback;
			}
			return Vector3{
				actionJson[key][0].get<float>(),
				actionJson[key][1].get<float>(),
				actionJson[key][2].get<float>()
			};
		};

		action->SetOpenOffsetPosition(   readVec3("openOffsetPosition",    { 0.0f, 5.0f, 0.0f }));
		action->SetOpenOffsetRotationDeg(readVec3("openOffsetRotationDeg", { 0.0f, 0.0f, 0.0f }));
		action->SetOpenOffsetScale(      readVec3("openOffsetScale",       { 0.0f, 0.0f, 0.0f }));
		action->SetOpenDuration(actionJson.value("openDuration", 1.0f));

		// 閉位置 (永続化) があれば読み込む
		if (actionJson.value("closedCaptured", false)) {
			action->SetClosedPose(
				readVec3("closedPosition", { 0.0f, 0.0f, 0.0f }),
				readVec3("closedRotation", { 0.0f, 0.0f, 0.0f }),
				readVec3("closedScale",    { 1.0f, 1.0f, 1.0f }));
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
		return action;
	}

	Logger("[TriggerActionFactory] 未知の type: " + type + "\n");
	return nullptr;
}
