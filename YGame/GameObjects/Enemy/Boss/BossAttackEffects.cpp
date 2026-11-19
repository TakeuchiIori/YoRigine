#include "BossAttackEffects.h"

// Engine
#include "Particle/EffectHandle.h"
#include "Systems/GameTime/GameTime.h"
#include <Vfx/VfxMesh/Runtime/TrailMeshEmitter.h>
#include <Vfx/VfxMesh/Runtime/VfxMeshSpawner.h>

namespace {
// ── 使うアセット（中身は JSON。名前だけをここで決める）──
// VfxMesh（Resources/Json/VfxMesh/）
constexpr const char *kTrailAsset =
    "Resources/Json/VfxMesh/FxBossSwingTrail.json";
constexpr const char *kImpactCore =
    "FxBossImpactCore"; // 土煙ドーム＋衝撃波リング＋閃光
constexpr const char *kShockBlast = "FxBossShockBlast"; // 地面を走る多重リング
constexpr const char *kSwingBurst = "FxBossSwingBurst"; // 手元の拳圧リング
constexpr const char *kHitFlash = "FxBossHitFlash";     // 命中の閃光

// 粒子（Resources/Json/YEffects/ のバンドル）
constexpr const char *kImpactSparks = "FxBossImpactSparks"; // 飛び散る火花
constexpr const char *kImpactDebris = "FxBossImpactDebris"; // 跳ね上がる破片
constexpr const char *kImpactDust = "FxBossImpactDust";     // 広がる土煙
constexpr const char *kSwingSparks = "FxBossSwingSparks";   // 手元の火花
constexpr const char *kHitSparks = "FxBossHitSparks";       // 命中の火花

// エミッタ側に設定した数だけ出す
constexpr int kUseEmitterCount = -1;
} // namespace

BossAttackEffects::BossAttackEffects() = default;
BossAttackEffects::~BossAttackEffects() = default;

void BossAttackEffects::Initialize(YoRigine::Camera *camera) {
  for (size_t i = 0; i < kHandCount; ++i) {
    trails_[i] = std::make_unique<YoRigine::TrailMeshEmitter>();
    trails_[i]->SetCamera(camera);
    trails_[i]->LoadAsset(kTrailAsset);
    trailPlaying_[i] = false;
  }
}

/*==========================================================================
手の軌跡
//========================================================================*/
void BossAttackEffects::UpdateHandTrail(Hand hand, const Vector3 &tip,
                                        const Vector3 &root, bool active) {
  const size_t index = static_cast<size_t>(hand);
  auto &trail = trails_[index];
  if (!trail) {
    return;
  }

  if (active && !trailPlaying_[index]) {
    trail->Play();
    trailPlaying_[index] = true;
  } else if (!active && trailPlaying_[index]) {
    // 止めた後も出ている軌跡は寿命まで残り、自然に消える
    trail->Stop();
    trailPlaying_[index] = false;
  }
  trail->AddPoint(tip, root);
}

void BossAttackEffects::Update(float deltaTime) {
  for (auto &trail : trails_) {
    if (trail) {
      trail->Update(deltaTime);
    }
  }
}

void BossAttackEffects::Draw() {
  for (auto &trail : trails_) {
    if (trail) {
      trail->Draw();
    }
  }
}

void BossAttackEffects::StopTrails() {
  for (size_t i = 0; i < kHandCount; ++i) {
    if (trails_[i] && trailPlaying_[i]) {
      trails_[i]->Stop();
    }
    trailPlaying_[i] = false;
  }
}

/*==========================================================================
一発もの
//========================================================================*/
void BossAttackEffects::PlaySwingBurst(const Vector3 &handPosition) {
  VfxMeshSpawner::GetInstance()->Spawn(kSwingBurst, handPosition);
  EffectHandle::PlayOneShot(kSwingSparks, handPosition, kUseEmitterCount);
}

void BossAttackEffects::PlayGroundImpact(const Vector3 &groundPosition,
                                         float scale) {
  // メッシュ系（土煙ドーム・リング・閃光）は大きさを変えられるので倍率を渡す。
  // 粒子は数と速さで迫力を作っているので、そのまま出す。
  VfxMeshSpawner::GetInstance()->Spawn(kImpactCore, groundPosition, scale);
  EffectHandle::PlayOneShot(kImpactSparks, groundPosition, kUseEmitterCount);
  EffectHandle::PlayOneShot(kImpactDebris, groundPosition, kUseEmitterCount);
  EffectHandle::PlayOneShot(kImpactDust, groundPosition, kUseEmitterCount);
}

void BossAttackEffects::PlayShockBlast(const Vector3 &groundPosition,
                                       float radius) {
  // アセットは半径 1m で作ってあるので、判定の半径をそのまま倍率にすると
  // 見えているリングと実際に当たる範囲が一致する
  VfxMeshSpawner::GetInstance()->Spawn(kShockBlast, groundPosition, radius);
  EffectHandle::PlayOneShot(kImpactDust, groundPosition, kUseEmitterCount);
  EffectHandle::PlayOneShot(kImpactSparks, groundPosition, kUseEmitterCount);
}

void BossAttackEffects::PlayHitBurst(const Vector3 &hitPosition, bool guarded) {
  // ガードされた時は閃光だけ小さく出す（素通しの命中と見分けがつくように）
  VfxMeshSpawner::GetInstance()->Spawn(kHitFlash, hitPosition,
                                       guarded ? 0.6f : 1.0f);
  if (!guarded) {
    EffectHandle::PlayOneShot(kHitSparks, hitPosition, kUseEmitterCount);
  } else {
    EffectHandle::PlayOneShot(kSwingSparks, hitPosition, kUseEmitterCount);
  }
}
