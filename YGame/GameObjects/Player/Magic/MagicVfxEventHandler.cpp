#include "MagicVfxEventHandler.h"

#include "Collision/Core/BaseCollider.h"
#include "GameObjects/Player/Camera/PlayerCamera.h"
#include "GameObjects/Player/Player.h"
#include "MagicCastGeometry.h"
#include "Particle/EffectHandle.h"
#include "GPUParticle/GpuEmitManager.h"
#include "Vfx/VfxMesh/Runtime/VfxMeshHandle.h"
#include "Vfx/VfxMesh/Runtime/VfxMeshSpawner.h"

#include <algorithm>
#include <cmath>

void MagicVfxEventHandler::Execute(const MagicTimelineEvent &event,
                                   const MagicEventContext &context) {
  if (!context.owner)
    return;

  Player &player = *context.owner;
  const Vector3 origin = MagicCastGeometry::ResolveCastOrigin(player);
  const Vector3 target = ResolveTargetPoint(player, event, origin);
  const float scale = ResolveScale(event, context.chargeTime);
  const float timeScale = ResolveTimeScale(event);

  // CPU/GPU パーティクルはイベント種類から発生基準位置を選び、
  // 同じタイムラインからワンショット放出する。
  const bool atTarget = event.type == MagicEventType::SpawnArea ||
                        event.type == MagicEventType::StrikeTarget;
  const Vector3 effectPosition = (atTarget ? target : origin) + event.effectOffset;
  if (event.effectBackend == MagicEffectBackend::CpuParticle) {
    if (!event.vfxAsset.empty()) {
      EffectHandle::PlayOneShot(event.vfxAsset, effectPosition,
                                std::max(1, event.emitCount));
    }
    return;
  }
  if (event.effectBackend == MagicEffectBackend::GpuParticle) {
    if (!event.vfxAsset.empty()) {
      YoRigine::GpuEmitManager::GetInstance()->EmitGroups(
          event.vfxAsset, effectPosition,
          static_cast<float>(std::max(1, event.emitCount)));
    }
    return;
  }

  switch (event.type) {
  case MagicEventType::PlayVfx:
    VfxMeshHandle::PlayOneShot(ResolveAssetName(event, "Smoke"), origin,
                               std::max(scale * 0.35f, 0.2f), timeScale);
    break;
  case MagicEventType::SpawnBeam:
    VfxMeshHandle::PlayBolt(ResolveAssetName(event, "Lightning"), origin,
                            target, false, timeScale);
    break;
  case MagicEventType::SpawnProjectile:
    // まだ弾オブジェクトを所有しない段階では、入力データの「発射」を
    // 起点→着弾点の軌跡VFXと着弾VFXへ変換する。弾寿命・追尾・判定は次の層で足す。
    VfxMeshHandle::PlayBolt("Lightning", origin, target, false, timeScale);
    VfxMeshHandle::PlayOneShot(ResolveAssetName(event, "Explosion"), target,
                               scale, timeScale);
    break;
  case MagicEventType::SpawnArea:
    VfxMeshHandle::PlayOneShot(ResolveAssetName(event, "Explosion"), target,
                               std::max(event.radius, scale), timeScale);
    break;
  case MagicEventType::StrikeTarget:
    VfxMeshHandle::PlayBolt(ResolveAssetName(event, "Lightning"),
                            target + Vector3{0.0f, 12.0f, 0.0f}, target, false,
                            timeScale);
    VfxMeshHandle::PlayOneShot("Explosion", target, scale, timeScale);
    break;
  default:
    break;
  }
}

Vector3
MagicVfxEventHandler::ResolveTargetPoint(Player &player,
                                         const MagicTimelineEvent &event,
                                         const Vector3 &origin) const {
  if (PlayerCamera *camera = player.GetPlayerCamera()) {
    if (BaseCollider *target = camera->GetLockedTarget()) {
      return target->GetCenterPosition();
    }
  }

  return origin +
         MagicCastGeometry::ResolveForward(player) * ResolveRange(event, 0.0f);
}

float MagicVfxEventHandler::ResolveRange(const MagicTimelineEvent &event,
                                         float chargeTime) const {
  const float dataRange = event.speed > 0.0f ? event.speed : 18.0f;
  const float chargeBonus = std::clamp(chargeTime, 0.0f, 2.0f) * 4.0f;
  return dataRange + chargeBonus;
}

float MagicVfxEventHandler::ResolveScale(const MagicTimelineEvent &event,
                                         float chargeTime) const {
  const float baseScale =
      event.radius > 0.0f ? event.radius : std::max(event.power * 0.15f, 1.0f);
  return baseScale + std::clamp(chargeTime, 0.0f, 2.0f) * 0.35f;
}

float MagicVfxEventHandler::ResolveTimeScale(
    const MagicTimelineEvent &event) const {
  if (event.duration <= 0.0f)
    return 1.0f;

  // 現行の Lightning は約0.25秒のワンショットなので、データ側の duration を
  // 「見せたい尺」として扱い、アセット時間を伸縮する。厳密な寿命管理はVFX層に閉じる。
  return std::clamp(0.25f / event.duration, 0.1f, 4.0f);
}

std::string
MagicVfxEventHandler::ResolveAssetName(const MagicTimelineEvent &event,
                                       const std::string &fallback) const {
  // VFX は Editor の専用欄から明示的に選ぶ。
  if (!event.vfxAsset.empty() &&
      VfxMeshSpawner::GetInstance()->GetAsset(event.vfxAsset)) {
    return event.vfxAsset;
  }

  // 旧データは label が VFX 名も兼ねていたため、後方互換でのみ参照。
  if (!event.label.empty() &&
      VfxMeshSpawner::GetInstance()->GetAsset(event.label)) {
    return event.label;
  }
  return fallback;
}
