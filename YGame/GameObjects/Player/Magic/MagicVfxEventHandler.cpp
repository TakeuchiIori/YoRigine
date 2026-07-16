#include "MagicVfxEventHandler.h"

#include "Collision/Core/BaseCollider.h"
#include "GameObjects/Enemy/BattleEnemy/BattleEnemy.h"
#include "GameObjects/Enemy/BattleEnemy/BattleEnemyManager.h"
#include "GameObjects/Player/Camera/PlayerCamera.h"
#include "GameObjects/Player/Player.h"
#include "MagicCastGeometry.h"
#include "Particle/EffectHandle.h"
#include "GPUParticle/GpuEmitManager.h"
#include "Composite/CompositeEffectManager.h"
#include "Systems/Camera/Virtuals/FollowCamera/FollowCamera.h"
#include "Systems/GameTime/GameTime.h"
#include "UI/Damage/DamageNumberManager.h"
#include "Vfx/VfxMesh/Runtime/VfxMeshHandle.h"
#include "Vfx/VfxMesh/Runtime/VfxMeshSpawner.h"

#include <algorithm>
#include <cmath>

namespace {
// 全体落雷（コンボフィニッシャー）の落下開始高さと演出の手応え。
constexpr float kStrikeAllBoltHeight = 14.0f; // 落雷ビームの始点高さ
constexpr float kStrikeAllHitStop = 0.06f;    // 全体ヒットの瞬間だけ軽く止める
constexpr float kStrikeAllShakeIntensity = 0.28f;
constexpr float kStrikeAllShakeDuration = 0.22f;
} // namespace

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
  if (event.effectBackend == MagicEffectBackend::Composite) {
    if (!event.vfxAsset.empty()) {
      CompositeEffectManager::PlayParams params;
      params.minDuration = std::max(0.0f, event.duration);
      CompositeEffectManager::GetInstance()->PlayOneShot(
          event.vfxAsset, effectPosition, params);
    }
    return;
  }
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
    // 紫Smokeを暗黙の既定値にしない。必要な場合だけアセット欄で明示選択する。
    VfxMeshHandle::PlayOneShot(ResolveAssetName(event, "NewEffect3"), origin,
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
  case MagicEventType::SpawnArea: {
    // 範囲攻撃は地面に沿わせる（対象が浮いていても・半分埋まっていても地表に出す）
    const Vector3 ground = MagicCastGeometry::ResolveGroundPoint(target);
    VfxMeshHandle::PlayOneShot(ResolveAssetName(event, "Explosion"), ground,
                               std::max(event.radius, scale), timeScale);
    break;
  }
  case MagicEventType::StrikeTarget: {
    // 落雷は敵の真下の地面へ落とす（着弾リングが地中に沈まないように）
    const Vector3 ground = MagicCastGeometry::ResolveGroundPoint(target);
    VfxMeshHandle::PlayBolt(ResolveAssetName(event, "Lightning"),
                            ground + Vector3{0.0f, 12.0f, 0.0f}, ground, false,
                            timeScale);
    VfxMeshHandle::PlayOneShot("Explosion", ground, scale, timeScale);
    break;
  }
  case MagicEventType::StrikeAllEnemies:
    ExecuteStrikeAllEnemies(player, event, scale, timeScale);
    break;
  default:
    break;
  }
}

// ============================================================
// 全体落雷（コンボフィニッシャー）
//   バトル中の生存敵すべてへ上空から落雷し、各着地点（地面高さに
//   クランプ）へ衝撃Composite(vfxAsset)を出す。event.power をダメージ
//   として各敵へ適用する（この形態は弾インスタンスを持たないため、
//   例外的にここでダメージまで面倒を見る）。
// ============================================================
void MagicVfxEventHandler::ExecuteStrikeAllEnemies(
    Player &player, const MagicTimelineEvent &event, float scale,
    float timeScale) {
  BattleEnemyManager *manager = BattleEnemyManager::GetCurrent();
  if (!manager)
    return;

  const std::vector<BattleEnemy *> enemies = manager->GetActiveBattleEnemies();
  if (enemies.empty())
    return;

  const int damage = static_cast<int>(std::max(0.0f, event.power));
  bool anyStruck = false;

  for (BattleEnemy *enemy : enemies) {
    if (!enemy || !enemy->IsAlive())
      continue;

    // 着地点は敵の直下の地面（埋もれ防止）。落雷はその真上から落とす。
    const Vector3 ground =
        MagicCastGeometry::ResolveGroundPoint(enemy->GetTranslate());
    const Vector3 sky = ground + Vector3{0.0f, kStrikeAllBoltHeight, 0.0f};

    // 落雷ビーム＋地面の衝撃Composite（雷リング・スパーク等はアセット側で束ねる）
    VfxMeshHandle::PlayBolt("Lightning", sky, ground, false, timeScale);
    if (!event.vfxAsset.empty()) {
      CompositeEffectManager::PlayParams params;
      params.minDuration = std::max(0.0f, event.duration);
      CompositeEffectManager::GetInstance()->PlayOneShot(event.vfxAsset,
                                                         ground, params);
    } else {
      VfxMeshHandle::PlayOneShot("Explosion", ground, scale, timeScale);
    }

    if (damage > 0) {
      enemy->TakeDamage(damage);
      DamageNumberManager::GetInstance()->SpawnDamage(
          damage, enemy->GetTranslate(), false);
    }
    anyStruck = true;
  }

  // 手応え（全体ヒットで1回だけ）。フィニッシャーの重みを出す。
  if (anyStruck) {
    YoRigine::GameTime::SetHitStop(kStrikeAllHitStop);
    if (FollowCamera *camera = player.GetFollowCamera()) {
      camera->StartShake(kStrikeAllShakeIntensity, kStrikeAllShakeDuration);
    }
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
