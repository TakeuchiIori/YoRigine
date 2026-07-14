#pragma once

#include <string>
#include <vector>
#include <json.hpp>
#include "Debugger/CurveEditor/CurveChannel.h"

enum class PlayerMagicSlot {
	Primary,
	Secondary,
	Utility,
};

enum class MagicInputMode {
	Tap,
	ChargeRelease,
};

enum class MagicEventTrigger {
	OnStart,
	OnTimeline,
	OnRelease,
};

enum class MagicElement {
	None,
	Fire,
	Thunder,
	Ice,
	Light,
};

enum class MagicEventType {
	Debug,
	PlayVfx,
	SpawnBeam,
	SpawnProjectile,
	SpawnArea,
	StrikeTarget,
};

enum class MagicTrajectoryType {
	Forward,
	LockOnOrForward,
	StrikeTarget,
};

enum class MagicHitShape {
	Sphere,
};

struct MagicTimelineEvent {
	float time = 0.0f;
	MagicEventTrigger trigger = MagicEventTrigger::OnTimeline;
	MagicEventType type = MagicEventType::Debug;
	MagicElement element = MagicElement::None;
	std::string label;
	float power = 0.0f;
	float duration = 0.0f;
	float radius = 0.0f;
	float speed = 0.0f;
	bool fired = false;
};

struct MagicActionData {
	std::string name = "DebugMagic";
	PlayerMagicSlot slot = PlayerMagicSlot::Primary;
	MagicInputMode inputMode = MagicInputMode::Tap;
	std::string animationName;
	MagicTrajectoryType trajectoryType = MagicTrajectoryType::LockOnOrForward;
	MagicHitShape hitShape = MagicHitShape::Sphere;
	float duration = 0.35f;
	float range = 18.0f;
	float hitRadius = 1.5f;
	float hitDelay = 0.0f;
	float minChargeTime = 0.0f;
	float maxChargeTime = 1.0f;
	float cooldown = 0.15f;
	float chainResetTime = 1.0f;
	CurveChannel scaleCurve{ "Scale", 0.0f, 3.0f };
	std::vector<MagicTimelineEvent> events;
};

const char* ToString(PlayerMagicSlot slot);
const char* ToString(MagicInputMode mode);
const char* ToString(MagicEventTrigger trigger);
const char* ToString(MagicElement element);
const char* ToString(MagicEventType type);
const char* ToString(MagicTrajectoryType type);
const char* ToString(MagicHitShape shape);

PlayerMagicSlot PlayerMagicSlotFromString(const std::string& value);
MagicInputMode MagicInputModeFromString(const std::string& value);
MagicEventTrigger MagicEventTriggerFromString(const std::string& value);
MagicElement MagicElementFromString(const std::string& value);
MagicEventType MagicEventTypeFromString(const std::string& value);
MagicTrajectoryType MagicTrajectoryTypeFromString(const std::string& value);
MagicHitShape MagicHitShapeFromString(const std::string& value);

void to_json(nlohmann::json& j, const MagicTimelineEvent& event);
void from_json(const nlohmann::json& j, MagicTimelineEvent& event);
void to_json(nlohmann::json& j, const MagicActionData& action);
void from_json(const nlohmann::json& j, MagicActionData& action);
