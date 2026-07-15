#pragma once

#include "Debugger/CurveEditor/CurveChannel.h"
#include <json.hpp>
#include <string>
#include <vector>

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
  float damage = 10.0f;
  float hitRadius = 1.5f;
  float hitDelay = 0.0f;
  float hitInterval =
      0.0f; // 0以下: 1体1回のみ。0超: この間隔で同じ敵に再ヒット（設置型用）
  float minChargeTime = 0.0f;
  float maxChargeTime = 1.0f;
  float cooldown = 0.15f;
  float chainResetTime = 1.0f;
  // ヒット時の手応え。剣攻撃と同じ GameTime::SetHitStop /
  // FollowCamera::StartShake を使う。
  // 1インスタンスの最初の命中でのみ発火し、エリア再ヒットでは連発しない。
  float hitStopDuration = 0.0f;
  float shakeIntensity = 0.0f;
  float shakeDuration = 0.0f;
  // 飛道弾の見た目。当たり判定(MagicAttackInstance)本体に追従させ、
  // VFXと判定の位置・寿命を同期させる。空なら見た目なし＝従来の透明判定のまま。
  std::string travelVfx;     // 飛翔中に本体へ追従させるループVFX
  std::string impactVfx;     // 着弾/消滅時に一度だけ出すVFX
  bool destroyOnHit = false; // true: 敵に最初に当たった時点で消滅（貫通しない）
  CurveChannel scaleCurve{"Scale", 0.0f, 3.0f};
  std::vector<MagicTimelineEvent> events;
};

const char *ToString(PlayerMagicSlot slot);
const char *ToString(MagicInputMode mode);
const char *ToString(MagicEventTrigger trigger);
const char *ToString(MagicElement element);
const char *ToString(MagicEventType type);
const char *ToString(MagicTrajectoryType type);
const char *ToString(MagicHitShape shape);

PlayerMagicSlot PlayerMagicSlotFromString(const std::string &value);
MagicInputMode MagicInputModeFromString(const std::string &value);
MagicEventTrigger MagicEventTriggerFromString(const std::string &value);
MagicElement MagicElementFromString(const std::string &value);
MagicEventType MagicEventTypeFromString(const std::string &value);
MagicTrajectoryType MagicTrajectoryTypeFromString(const std::string &value);
MagicHitShape MagicHitShapeFromString(const std::string &value);

void to_json(nlohmann::json &j, const MagicTimelineEvent &event);
void from_json(const nlohmann::json &j, MagicTimelineEvent &event);
void to_json(nlohmann::json &j, const MagicActionData &action);
void from_json(const nlohmann::json &j, MagicActionData &action);
