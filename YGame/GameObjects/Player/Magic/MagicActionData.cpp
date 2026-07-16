#include "MagicActionData.h"

namespace {
template <typename T>
T EnumFromString(const std::string &value,
                 const std::initializer_list<std::pair<const char *, T>> &table,
                 T fallback) {
  for (const auto &[name, enumValue] : table) {
    if (value == name)
      return enumValue;
  }
  return fallback;
}
} // namespace

const char *ToString(PlayerMagicSlot slot) {
  switch (slot) {
  case PlayerMagicSlot::Primary:
    return "Primary";
  case PlayerMagicSlot::Secondary:
    return "Secondary";
  case PlayerMagicSlot::Utility:
    return "Utility";
  default:
    return "Primary";
  }
}

const char *ToString(MagicInputMode mode) {
  switch (mode) {
  case MagicInputMode::Tap:
    return "Tap";
  case MagicInputMode::ChargeRelease:
    return "ChargeRelease";
  default:
    return "Tap";
  }
}

const char *ToString(MagicEventTrigger trigger) {
  switch (trigger) {
  case MagicEventTrigger::OnStart:
    return "OnStart";
  case MagicEventTrigger::OnTimeline:
    return "OnTimeline";
  case MagicEventTrigger::OnRelease:
    return "OnRelease";
  default:
    return "OnTimeline";
  }
}

const char *ToString(MagicElement element) {
  switch (element) {
  case MagicElement::None:
    return "None";
  case MagicElement::Fire:
    return "Fire";
  case MagicElement::Thunder:
    return "Thunder";
  case MagicElement::Ice:
    return "Ice";
  case MagicElement::Light:
    return "Light";
  default:
    return "None";
  }
}

const char *ToString(MagicEventType type) {
  switch (type) {
  case MagicEventType::Debug:
    return "Debug";
  case MagicEventType::PlayVfx:
    return "PlayVfx";
  case MagicEventType::SpawnBeam:
    return "SpawnBeam";
  case MagicEventType::SpawnProjectile:
    return "SpawnProjectile";
  case MagicEventType::SpawnArea:
    return "SpawnArea";
  case MagicEventType::StrikeTarget:
    return "StrikeTarget";
  default:
    return "Debug";
  }
}

const char *ToString(MagicEffectBackend backend) {
  switch (backend) {
  case MagicEffectBackend::Composite: return "Composite";
  case MagicEffectBackend::VfxMesh: return "VfxMesh";
  case MagicEffectBackend::CpuParticle: return "CpuParticle";
  case MagicEffectBackend::GpuParticle: return "GpuParticle";
  default: return "VfxMesh";
  }
}

const char *ToString(MagicTrajectoryType type) {
  switch (type) {
  case MagicTrajectoryType::Forward:
    return "Forward";
  case MagicTrajectoryType::LockOnOrForward:
    return "LockOnOrForward";
  case MagicTrajectoryType::StrikeTarget:
    return "StrikeTarget";
  default:
    return "LockOnOrForward";
  }
}

const char *ToString(MagicHitShape shape) {
  switch (shape) {
  case MagicHitShape::Sphere:
    return "Sphere";
  case MagicHitShape::AABB:
    return "AABB";
  default:
    return "Sphere";
  }
}

PlayerMagicSlot PlayerMagicSlotFromString(const std::string &value) {
  return EnumFromString<PlayerMagicSlot>(
      value,
      {{"Primary", PlayerMagicSlot::Primary},
       {"Secondary", PlayerMagicSlot::Secondary},
       {"Utility", PlayerMagicSlot::Utility}},
      PlayerMagicSlot::Primary);
}

MagicInputMode MagicInputModeFromString(const std::string &value) {
  return EnumFromString<MagicInputMode>(
      value,
      {{"Tap", MagicInputMode::Tap},
       {"ChargeRelease", MagicInputMode::ChargeRelease}},
      MagicInputMode::Tap);
}

MagicEventTrigger MagicEventTriggerFromString(const std::string &value) {
  return EnumFromString<MagicEventTrigger>(
      value,
      {{"OnStart", MagicEventTrigger::OnStart},
       {"OnTimeline", MagicEventTrigger::OnTimeline},
       {"OnRelease", MagicEventTrigger::OnRelease}},
      MagicEventTrigger::OnTimeline);
}

MagicElement MagicElementFromString(const std::string &value) {
  return EnumFromString<MagicElement>(value,
                                      {{"None", MagicElement::None},
                                       {"Fire", MagicElement::Fire},
                                       {"Thunder", MagicElement::Thunder},
                                       {"Ice", MagicElement::Ice},
                                       {"Light", MagicElement::Light}},
                                      MagicElement::None);
}

MagicEventType MagicEventTypeFromString(const std::string &value) {
  return EnumFromString<MagicEventType>(
      value,
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

MagicEffectBackend MagicEffectBackendFromString(const std::string &value) {
  return EnumFromString<MagicEffectBackend>(
      value,
      {{"Composite", MagicEffectBackend::Composite},
       {"VfxMesh", MagicEffectBackend::VfxMesh},
       {"CpuParticle", MagicEffectBackend::CpuParticle},
       {"GpuParticle", MagicEffectBackend::GpuParticle}},
      MagicEffectBackend::VfxMesh);
}

MagicTrajectoryType MagicTrajectoryTypeFromString(const std::string &value) {
  return EnumFromString<MagicTrajectoryType>(
      value,
      {{"Forward", MagicTrajectoryType::Forward},
       {"LockOnOrForward", MagicTrajectoryType::LockOnOrForward},
       {"StrikeTarget", MagicTrajectoryType::StrikeTarget}},
      MagicTrajectoryType::LockOnOrForward);
}

MagicHitShape MagicHitShapeFromString(const std::string &value) {
  return EnumFromString<MagicHitShape>(
      value,
      {{"Sphere", MagicHitShape::Sphere}, {"AABB", MagicHitShape::AABB}},
      MagicHitShape::Sphere);
}

void to_json(nlohmann::json &j, const MagicTimelineEvent &event) {
  j = nlohmann::json{
      {"time", event.time},           {"trigger", ToString(event.trigger)},
      {"type", ToString(event.type)}, {"element", ToString(event.element)},
      {"label", event.label},         {"vfxAsset", event.vfxAsset},
      {"effectBackend", ToString(event.effectBackend)},
      {"effectOffset", {event.effectOffset.x, event.effectOffset.y,
                          event.effectOffset.z}},
      {"emitCount", event.emitCount},
      {"power", event.power},
      {"duration", event.duration},   {"radius", event.radius},
      {"speed", event.speed},
  };
}

void from_json(const nlohmann::json &j, MagicTimelineEvent &event) {
  event.time = j.value("time", 0.0f);
  event.trigger = MagicEventTriggerFromString(j.value("trigger", "OnTimeline"));
  event.type = MagicEventTypeFromString(j.value("type", "Debug"));
  event.element = MagicElementFromString(j.value("element", "None"));
  event.label = j.value("label", "");
  event.vfxAsset = j.value("vfxAsset", "");
  event.effectBackend = MagicEffectBackendFromString(
      j.value("effectBackend", "VfxMesh"));
  if (j.contains("effectOffset") && j.at("effectOffset").is_array() &&
      j.at("effectOffset").size() >= 3) {
    const auto &v = j.at("effectOffset");
    event.effectOffset = {v[0].get<float>(), v[1].get<float>(),
                          v[2].get<float>()};
  } else {
    event.effectOffset = {0.0f, 0.0f, 0.0f};
  }
  event.emitCount = j.value("emitCount", 20);
  event.power = j.value("power", 0.0f);
  event.duration = j.value("duration", 0.0f);
  event.radius = j.value("radius", 0.0f);
  event.speed = j.value("speed", 0.0f);
  event.fired = false;
}

void to_json(nlohmann::json &j, const MagicActionData &action) {
  j = nlohmann::json{
      {"name", action.name},
      {"slot", ToString(action.slot)},
      {"inputMode", ToString(action.inputMode)},
      {"animationName", action.animationName},
      {"trajectoryType", ToString(action.trajectoryType)},
      {"hitShape", ToString(action.hitShape)},
      {"duration", action.duration},
      {"range", action.range},
      {"damage", action.damage},
      {"hitRadius", action.hitRadius},
      {"hitOffset", {action.hitOffset.x, action.hitOffset.y, action.hitOffset.z}},
      {"hitHalfExtents", {action.hitHalfExtents.x, action.hitHalfExtents.y,
                           action.hitHalfExtents.z}},
      {"hitInterval", action.hitInterval},
      {"hitDelay", action.hitDelay},
      {"minChargeTime", action.minChargeTime},
      {"maxChargeTime", action.maxChargeTime},
      {"cooldown", action.cooldown},
      {"chainResetTime", action.chainResetTime},
      {"hitStopDuration", action.hitStopDuration},
      {"shakeIntensity", action.shakeIntensity},
      {"shakeDuration", action.shakeDuration},
      {"travelVfx", action.travelVfx},
      {"impactVfx", action.impactVfx},
      {"destroyOnHit", action.destroyOnHit},
      {"scaleCurve", action.scaleCurve.SaveToJson()},
      {"events", action.events},
  };
}

void from_json(const nlohmann::json &j, MagicActionData &action) {
  action.name = j.value("name", "DebugMagic");
  action.slot = PlayerMagicSlotFromString(j.value("slot", "Primary"));
  action.inputMode = MagicInputModeFromString(j.value("inputMode", "Tap"));
  action.animationName = j.value("animationName", "");
  action.trajectoryType = MagicTrajectoryTypeFromString(
      j.value("trajectoryType", "LockOnOrForward"));
  action.hitShape = MagicHitShapeFromString(j.value("hitShape", "Sphere"));
  action.duration = j.value("duration", 0.35f);
  action.range = j.value("range", 18.0f);
  action.damage = j.value("damage", 10.0f);
  action.hitRadius = j.value("hitRadius", 1.5f);
  if (j.contains("hitOffset") && j.at("hitOffset").is_array() &&
      j.at("hitOffset").size() >= 3) {
    const auto &v = j.at("hitOffset");
    action.hitOffset = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
  } else {
    action.hitOffset = {0.0f, 0.0f, 0.0f};
  }
  if (j.contains("hitHalfExtents") && j.at("hitHalfExtents").is_array() &&
      j.at("hitHalfExtents").size() >= 3) {
    const auto &v = j.at("hitHalfExtents");
    action.hitHalfExtents = {v[0].get<float>(), v[1].get<float>(),
                             v[2].get<float>()};
  } else {
    action.hitHalfExtents = {1.5f, 1.5f, 4.0f};
  }
  action.hitDelay = j.value("hitDelay", 0.0f);
  action.hitInterval = j.value("hitInterval", 0.0f);
  action.minChargeTime = j.value("minChargeTime", 0.0f);
  action.maxChargeTime = j.value("maxChargeTime", 1.0f);
  action.cooldown = j.value("cooldown", 0.15f);
  action.chainResetTime = j.value("chainResetTime", 1.0f);
  action.hitStopDuration = j.value("hitStopDuration", 0.0f);
  action.shakeIntensity = j.value("shakeIntensity", 0.0f);
  action.shakeDuration = j.value("shakeDuration", 0.0f);
  action.travelVfx = j.value("travelVfx", "");
  action.impactVfx = j.value("impactVfx", "");
  action.destroyOnHit = j.value("destroyOnHit", false);
  if (j.contains("scaleCurve")) {
    action.scaleCurve.LoadFromJson(j.at("scaleCurve"));
  } else {
    action.scaleCurve.Reset(1.0f);
  }
  action.events = j.value("events", std::vector<MagicTimelineEvent>{});
}
