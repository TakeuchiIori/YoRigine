#pragma once

#include "Debugger/CurveEditor/CurveChannel.h"
#include "Vector3.h"
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
  StrikeAllEnemies, // バトル中の全敵へ上空から落雷＋地面衝撃（コンボフィニッシャー用）
};

enum class MagicEffectBackend {
  Composite,
  VfxMesh,
  CpuParticle,
  GpuParticle,
};

enum class MagicTrajectoryType {
  Forward,
  LockOnOrForward,
  StrikeTarget,
};

enum class MagicHitShape {
  Sphere,
  AABB,
};

struct MagicTimelineEvent {
  float time = 0.0f;
  MagicEventTrigger trigger = MagicEventTrigger::OnTimeline;
  MagicEventType type = MagicEventType::Debug;
  MagicElement element = MagicElement::None;
  std::string label;    // Editor用のメモ／イベント名
  float power = 0.0f;
  float duration = 0.0f;
  float radius = 0.0f;
  float speed = 0.0f;
  bool fired = false;
  // 既存の位置指定初期化と互換にするため末尾に追加。
  std::string vfxAsset; // 再生する VfxMesh アセット名
  MagicEffectBackend effectBackend = MagicEffectBackend::VfxMesh;
  Vector3 effectOffset = {0.0f, 0.0f, 0.0f};
  int emitCount = 20;
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
  // AABB 判定時の中心オフセットと各軸の半サイズ。
  // hitOffset は魔法本体の現在位置からのワールド軸オフセット。
  Vector3 hitOffset = {0.0f, 0.0f, 0.0f};
  Vector3 hitHalfExtents = {1.5f, 1.5f, 4.0f};
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
  // チャージ（ChargeRelease 専用）。長押し中に手元でループ再生し、
  // maxChargeTime 到達の瞬間に chargeMaxVfx を一度だけ出して「満タン」を伝える。
  std::string chargeVfx;    // 長押し中の手元ループVFX（Composite名）
  std::string chargeMaxVfx; // チャージ上限到達の合図に出すワンショットVFX
  // ヒットした敵へ追従させる状態VFX（例: 燃焼）。空なら何もしない。
  std::string hitStatusVfx;       // 敵に付着させるループVFX（Composite名）
  float hitStatusDuration = 0.0f; // 付着VFXの継続秒数
  CurveChannel scaleCurve{"Scale", 0.0f, 3.0f};
  std::vector<MagicTimelineEvent> events;
};

const char *ToString(PlayerMagicSlot slot);
const char *ToString(MagicInputMode mode);
const char *ToString(MagicEventTrigger trigger);
const char *ToString(MagicElement element);
const char *ToString(MagicEventType type);
const char *ToString(MagicEffectBackend backend);
const char *ToString(MagicTrajectoryType type);
const char *ToString(MagicHitShape shape);

PlayerMagicSlot PlayerMagicSlotFromString(const std::string &value);
MagicInputMode MagicInputModeFromString(const std::string &value);
MagicEventTrigger MagicEventTriggerFromString(const std::string &value);
MagicElement MagicElementFromString(const std::string &value);
MagicEventType MagicEventTypeFromString(const std::string &value);
MagicEffectBackend MagicEffectBackendFromString(const std::string &value);
MagicTrajectoryType MagicTrajectoryTypeFromString(const std::string &value);
MagicHitShape MagicHitShapeFromString(const std::string &value);

void to_json(nlohmann::json &j, const MagicTimelineEvent &event);
void from_json(const nlohmann::json &j, MagicTimelineEvent &event);
void to_json(nlohmann::json &j, const MagicActionData &action);
void from_json(const nlohmann::json &j, MagicActionData &action);
