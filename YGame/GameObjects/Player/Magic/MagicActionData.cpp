#include "MagicActionData.h"

namespace {
	template <typename T>
	T EnumFromString(const std::string& value, const std::initializer_list<std::pair<const char*, T>>& table, T fallback)
	{
		for (const auto& [name, enumValue] : table) {
			if (value == name) return enumValue;
		}
		return fallback;
	}
}

const char* ToString(PlayerMagicSlot slot)
{
	switch (slot) {
	case PlayerMagicSlot::Primary: return "Primary";
	case PlayerMagicSlot::Secondary: return "Secondary";
	case PlayerMagicSlot::Utility: return "Utility";
	default: return "Primary";
	}
}

const char* ToString(MagicInputMode mode)
{
	switch (mode) {
	case MagicInputMode::Tap: return "Tap";
	case MagicInputMode::ChargeRelease: return "ChargeRelease";
	default: return "Tap";
	}
}

const char* ToString(MagicEventTrigger trigger)
{
	switch (trigger) {
	case MagicEventTrigger::OnStart: return "OnStart";
	case MagicEventTrigger::OnTimeline: return "OnTimeline";
	case MagicEventTrigger::OnRelease: return "OnRelease";
	default: return "OnTimeline";
	}
}

const char* ToString(MagicElement element)
{
	switch (element) {
	case MagicElement::None: return "None";
	case MagicElement::Fire: return "Fire";
	case MagicElement::Thunder: return "Thunder";
	case MagicElement::Ice: return "Ice";
	case MagicElement::Light: return "Light";
	default: return "None";
	}
}

const char* ToString(MagicEventType type)
{
	switch (type) {
	case MagicEventType::Debug: return "Debug";
	case MagicEventType::PlayVfx: return "PlayVfx";
	case MagicEventType::SpawnBeam: return "SpawnBeam";
	case MagicEventType::SpawnProjectile: return "SpawnProjectile";
	case MagicEventType::SpawnArea: return "SpawnArea";
	case MagicEventType::StrikeTarget: return "StrikeTarget";
	default: return "Debug";
	}
}

PlayerMagicSlot PlayerMagicSlotFromString(const std::string& value)
{
	return EnumFromString<PlayerMagicSlot>(value,
		{ {"Primary", PlayerMagicSlot::Primary}, {"Secondary", PlayerMagicSlot::Secondary}, {"Utility", PlayerMagicSlot::Utility} },
		PlayerMagicSlot::Primary);
}

MagicInputMode MagicInputModeFromString(const std::string& value)
{
	return EnumFromString<MagicInputMode>(value,
		{ {"Tap", MagicInputMode::Tap}, {"ChargeRelease", MagicInputMode::ChargeRelease} },
		MagicInputMode::Tap);
}

MagicEventTrigger MagicEventTriggerFromString(const std::string& value)
{
	return EnumFromString<MagicEventTrigger>(value,
		{ {"OnStart", MagicEventTrigger::OnStart}, {"OnTimeline", MagicEventTrigger::OnTimeline}, {"OnRelease", MagicEventTrigger::OnRelease} },
		MagicEventTrigger::OnTimeline);
}

MagicElement MagicElementFromString(const std::string& value)
{
	return EnumFromString<MagicElement>(value,
		{ {"None", MagicElement::None}, {"Fire", MagicElement::Fire}, {"Thunder", MagicElement::Thunder}, {"Ice", MagicElement::Ice}, {"Light", MagicElement::Light} },
		MagicElement::None);
}

MagicEventType MagicEventTypeFromString(const std::string& value)
{
	return EnumFromString<MagicEventType>(value,
		{
			{"Debug", MagicEventType::Debug},
			{"PlayVfx", MagicEventType::PlayVfx},
			{"SpawnBeam", MagicEventType::SpawnBeam},
			{"SpawnProjectile", MagicEventType::SpawnProjectile},
			{"SpawnArea", MagicEventType::SpawnArea},
			{"StrikeTarget", MagicEventType::StrikeTarget},
		},
		MagicEventType::Debug);
}

void to_json(nlohmann::json& j, const MagicTimelineEvent& event)
{
	j = nlohmann::json{
		{"time", event.time},
		{"trigger", ToString(event.trigger)},
		{"type", ToString(event.type)},
		{"element", ToString(event.element)},
		{"label", event.label},
		{"power", event.power},
		{"duration", event.duration},
		{"radius", event.radius},
		{"speed", event.speed},
	};
}

void from_json(const nlohmann::json& j, MagicTimelineEvent& event)
{
	event.time = j.value("time", 0.0f);
	event.trigger = MagicEventTriggerFromString(j.value("trigger", "OnTimeline"));
	event.type = MagicEventTypeFromString(j.value("type", "Debug"));
	event.element = MagicElementFromString(j.value("element", "None"));
	event.label = j.value("label", "");
	event.power = j.value("power", 0.0f);
	event.duration = j.value("duration", 0.0f);
	event.radius = j.value("radius", 0.0f);
	event.speed = j.value("speed", 0.0f);
	event.fired = false;
}

void to_json(nlohmann::json& j, const MagicActionData& action)
{
	j = nlohmann::json{
		{"name", action.name},
		{"slot", ToString(action.slot)},
		{"inputMode", ToString(action.inputMode)},
		{"minChargeTime", action.minChargeTime},
		{"maxChargeTime", action.maxChargeTime},
		{"cooldown", action.cooldown},
		{"events", action.events},
	};
}

void from_json(const nlohmann::json& j, MagicActionData& action)
{
	action.name = j.value("name", "DebugMagic");
	action.slot = PlayerMagicSlotFromString(j.value("slot", "Primary"));
	action.inputMode = MagicInputModeFromString(j.value("inputMode", "Tap"));
	action.minChargeTime = j.value("minChargeTime", 0.0f);
	action.maxChargeTime = j.value("maxChargeTime", 1.0f);
	action.cooldown = j.value("cooldown", 0.15f);
	action.events = j.value("events", std::vector<MagicTimelineEvent>{});
}
