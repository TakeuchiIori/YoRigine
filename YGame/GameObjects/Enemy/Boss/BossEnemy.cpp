#include "BossEnemy.h"
#include "BossStates.h"

#include "../AI/EnemyAIContext.h"
#include "../Attack/Runtime/EnemyAttackLibrary.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include "Debugger/Logger.h"
#include "Graphics/Drawer/LineManager/Line.h"
#include "MathFunc.h"
#include "Object3D/BaseObjectManager.h"
#include "Player/Player.h"
#include "Systems/GameTime/GameTime.h"
#include "Systems/Input/Input.h"
#include <Collision/AreaCollision/Base/AreaManager.h>
#include <Loaders/Json/JsonManager.h>
#include <UI/Damage/DamageNumberManager.h>

#include "ModelManager.h"
#include <json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

const char *BossStateToString(BossState state) {
  switch (state) {
  case BossState::Intro:      return "Intro";
  case BossState::Idle:       return "Idle";
  case BossState::Chase:      return "Chase";
  case BossState::Attack:     return "Attack";
  case BossState::Stagger:    return "Stagger";
  case BossState::PhaseShift: return "PhaseShift";
  case BossState::Dead:       return "Dead";
  default:                    return "Unknown";
  }
}

BossEnemy::BossEnemy() = default;

BossEnemy::~BossEnemy() {
  // マネージャが生成時に登録しているので、どの削除経路でも解除する
  BaseObjectManager::GetInstance()->Unregister(this);
  StopStatusVfx();
}

/*==========================================================================
初期化
//========================================================================*/
void BossEnemy::Initialize(YoRigine::Camera *camera) {
  camera_ = camera;
  obj_ = std::make_unique<YoRigine::Object3d>();
  obj_->Initialize();
  wt_.Initialize();
  visualWt_.Initialize();
  wt_.useAnchorPoint_ = true;
  visualWt_.useAnchorPoint_ = true;
  InitCollision();

  // BaseEnemy::UpdateAnimation（点滅やスケールの演出）用。骨格アニメとは別物
  animation_ = std::make_unique<ObjectAnimation>();
  animation_->SetBaseColor({1.0f, 1.0f, 1.0f, 1.0f});
  animation_->SetBaseScale({1.0f, 1.0f, 1.0f});

  healthBarUI_ = std::make_unique<EnemyHealthBarUI>(this, camera);
  healthBarUI_->Initialize();

  debugLine_ = std::make_unique<YoRigine::Line>();
  debugLine_->Initialize();
  debugLine_->SetCamera(camera);

  attackEffects_ = std::make_unique<BossAttackEffects>();
  attackEffects_->Initialize(camera);
}

void BossEnemy::InitializeBattleData(const BattleEnemyData &data,
                                     Vector3 position, const Vector3 &scale) {
  enemyData_ = data;
  SetupHP(data.hp);

  // クリップ名などの調整値を先に読む（モデル設定で待機クリップ名を使うため）
  InitJson();

  SetupSkeletalModel(data.modelPath, params_.idleClip);
  SetRootMotionJoint(params_.rootJoint);
  if (!HasSkeletalMotion()) {
    Logger(("[BossEnemy] 警告: " + data.modelPath +
            " はスケルトンを持たないため、攻撃アニメを再生できません\n")
               .c_str());
  }

  // モデル倍率を掛けた大きさで置く。OBB も wt_ のスケールに追従して大きくなる
  const Vector3 bodyScale = scale * params_.modelScale;
  wt_.translate_ = position;
  wt_.scale_ = bodyScale;
  wt_.UpdateMatrix();
  SyncVisualTransform();
  if (animation_) {
    animation_->SetBaseScale(bodyScale);
  }

  isAlive_ = true;
  phase_ = 0;
  poise_ = 0.0f;
  attackCooldowns_.clear();

  SetupStateMachine();
  Logger(("[BossEnemy] Initialized: ID=" + data.enemyId +
          ", HP=" + std::to_string(data.hp) + "\n")
             .c_str());
}

void BossEnemy::SetupStateMachine() {
  fsm_.RegisterState<BossIntroState>(BossState::Intro, *this);
  fsm_.RegisterState<BossIdleState>(BossState::Idle, *this);
  fsm_.RegisterState<BossChaseState>(BossState::Chase, *this);
  fsm_.RegisterState<BossAttackState>(BossState::Attack, *this);
  fsm_.RegisterState<BossStaggerState>(BossState::Stagger, *this);
  fsm_.RegisterState<BossPhaseShiftState>(BossState::PhaseShift, *this);
  fsm_.RegisterState<BossDeadState>(BossState::Dead, *this);

  // 状態遷移をログへ。エディタを開かなくても「今どこで止まっているか」が追える
  fsm_.SetStateChangeCallback([this](BossState from, BossState to) {
    Logger(("[BossEnemy] " + std::string(BossStateToString(from)) + " -> " +
            BossStateToString(to) + " (距離 " +
            std::to_string(GetDistanceToPlayer()) + "m)\n")
               .c_str());
  });
  fsm_.SetInitialState(params_.playIntro ? BossState::Intro : BossState::Idle);
}

void BossEnemy::InitCollision() {
  // 雑魚と同じ種別にする。剣・魔法・ロックオン・カメラは種別IDで敵を判定しているので、
  // 種別を分けるとそれら全部にボス用の分岐を足すことになる。
  obbCollider_ = ColliderFactory::Create<OBBCollider>(
      this, &wt_, camera_,
      static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy));
  obbCollider_->SetIsStatic(false);
  // 質量は InitJson で調整値（既定 100000）に置き換える。
  // プレイヤー(100)と同じ桁だと、歩くだけで巨体を押し込めてしまう。
  obbCollider_->SetMass(params_.bodyMass);
  obbCollider_->SetLockPenetrationY(true);
}

void BossEnemy::InitJson() {
  std::string identifier = enemyData_.enemyId;
  if (identifier.empty())
    identifier = "UnknownBoss";

  // 雑魚の個体ファイルと同じ置き場所。コライダーの大きさもここで調整する
  jsonManager_ = std::make_unique<YoRigine::JsonManager>(
      identifier, "Resources/Json/Objects/BattleEnemies");
  jsonManager_->SetCategory(identifier);
  obbCollider_->InitJson(jsonManager_.get());

  jsonManager_->SetTreePrefix("ボス:クリップ");
  jsonManager_->Register("待機", &params_.idleClip);
  jsonManager_->Register("歩き", &params_.walkClip);
  jsonManager_->Register("走り", &params_.runClip);
  jsonManager_->Register("登場", &params_.introClip);
  jsonManager_->Register("フェーズ移行", &params_.phaseShiftClip);
  jsonManager_->Register("死亡", &params_.deathClip);
  jsonManager_->Register("ルートジョイント", &params_.rootJoint);
  jsonManager_->ClearTreePrefix();

  jsonManager_->SetTreePrefix("ボス:見た目");
  jsonManager_->Register("モデル倍率", &params_.modelScale);
  jsonManager_->ClearTreePrefix();

  jsonManager_->SetTreePrefix("ボス:移動");
  jsonManager_->Register("旋回速度(rad/s)", &params_.turnSpeed);
  jsonManager_->Register("走り出す距離", &params_.runDistance);
  jsonManager_->Register("走り速度倍率", &params_.runSpeedMultiplier);
  jsonManager_->ClearTreePrefix();

  jsonManager_->SetTreePrefix("ボス:思考");
  jsonManager_->Register("思考時間 最短", &params_.thinkTimeMin);
  jsonManager_->Register("思考時間 最長", &params_.thinkTimeMax);
  jsonManager_->Register("追跡を諦める秒数", &params_.chaseGiveUpTime);
  jsonManager_->ClearTreePrefix();

  jsonManager_->SetTreePrefix("ボス:怯み");
  jsonManager_->Register("怯み値 上限", &params_.poiseMax);
  jsonManager_->Register("怯み値 回復/秒", &params_.poiseRecoverPerSec);
  jsonManager_->Register("怯み時間", &params_.staggerDuration);
  jsonManager_->Register("パリィ時の怯み時間", &params_.parryStaggerDuration);
  jsonManager_->ClearTreePrefix();

  jsonManager_->SetTreePrefix("ボス:フェーズ");
  jsonManager_->Register("第2フェーズ HP割合", &params_.phase2HpRatio);
  jsonManager_->Register("第3フェーズ HP割合", &params_.phase3HpRatio);
  jsonManager_->Register("登場演出", &params_.playIntro);
  jsonManager_->ClearTreePrefix();

  jsonManager_->SetTreePrefix("ボス:カメラ");
  jsonManager_->Register("注視ジョイント", &params_.cameraFocusJoint);
  jsonManager_->Register("頭ジョイント", &params_.cameraTopJoint);
  jsonManager_->ClearTreePrefix();

  jsonManager_->SetTreePrefix("ボス:当たり判定");
  jsonManager_->Register("プレイヤー半径", &params_.playerHitRadius);
  jsonManager_->Register("プレイヤー高さ", &params_.playerHitHeight);
  jsonManager_->Register("攻撃が届く高さ", &params_.hitReachHeight);
  jsonManager_->Register("質量", &params_.bodyMass);
  jsonManager_->ClearTreePrefix();

  jsonManager_->SetTreePrefix("ボス:攻撃演出");
  jsonManager_->Register("叩きつけとみなす高さ", &params_.groundImpactHeight);
  jsonManager_->Register("衝撃波の半径", &params_.impactDamageRadius);
  jsonManager_->Register("衝撃波の威力", &params_.impactDamageMultiplier);
  jsonManager_->Register("衝撃エフェクトの大きさ", &params_.impactEffectScale);
  jsonManager_->Register("衝撃波リングにする半径", &params_.blastRadiusThreshold);
  jsonManager_->Register("衝撃の揺れ", &params_.impactShake);
  jsonManager_->Register("揺れが届く距離", &params_.impactShakeRange);
  jsonManager_->Register("命中の揺れ", &params_.hitShake);
  jsonManager_->ClearTreePrefix();

  jsonManager_->SetTreePrefix("ボス:出現");
  jsonManager_->Register("実体化の縁の色", &emergenceEdgeColor_);
  jsonManager_->ClearTreePrefix();

  jsonManager_->SetTreePrefix("ディゾルブ");
  jsonManager_->Register("継続時間(秒)", &dissolveDuration_);
  jsonManager_->Register("エッジ幅", &dissolveEdgeWidth_);
  jsonManager_->Register("エッジ発光色", &dissolveEdgeColor_);
  jsonManager_->Register("ノイズスケール", &dissolveNoiseScale_);
  jsonManager_->ClearTreePrefix();

  // 質量は JSON を読んだ後に反映する（InitCollision の時点ではまだ既定値のため）
  if (obbCollider_) {
    obbCollider_->SetMass(params_.bodyMass);
  }
}

/*==========================================================================
更新
//========================================================================*/
void BossEnemy::Update() {
  if (player_ && player_->GetCombat()->IsDead())
    return;

  const float dt = YoRigine::GameTime::GetDeltaTime();

  // 死亡は他のどの判断よりも優先する
  if (currentHp_ == 0 && GetState() != BossState::Dead) {
    ChangeState(BossState::Dead);
  } else {
    CheckPhaseTransition();
  }

  MarkPreviousPosition();

  // 技ごとのクールダウン
  for (auto &[id, remaining] : attackCooldowns_) {
    remaining = std::max(0.0f, remaining - dt);
  }

  // 怯み値は時間で抜けていく。怯み中は回復させない（崩れた直後に満タンへ戻るため）
  if (GetState() != BossState::Stagger) {
    poise_ = std::max(0.0f, poise_ - params_.poiseRecoverPerSec * dt);
  }

  UpdateAnimation(dt); // 色・スケールの演出（BaseEnemy）
  UpdateEmergence(dt); // 霧からの実体化
  if (!aiPaused_) {
    fsm_.Update(dt);
  }
  UpdateReactions(dt); // のけぞり・状態VFX

  AreaManager::GetInstance()->UpdateSingleObject(&wt_);

  UpdateVelocity();
  wt_.UpdateMatrix();
  SyncVisualTransform();

  // 骨格アニメを進める。当たり判定はこの後に調べるので、腕の位置は今フレームのもの
  if (obj_) {
    obj_->UpdateAnimation();
  }

  // 腰に焼き込まれた移動を本体へ移す。メッシュ・コライダー・判定が一緒に動く
  const Vector3 rootStep = ConsumeRootMotion();
  if (LengthSquared(rootStep) > 0.0f) {
    wt_.translate_ += rootStep;
    // 攻撃中の位置は AttackPlayer が毎フレーム書き直すので、そちらにも積む
    if (activeAttackPlayer_) {
      activeAttackPlayer_->AddWorldOffset(rootStep);
    }
    AreaManager::GetInstance()->UpdateSingleObject(&wt_);
    wt_.UpdateMatrix();
    SyncVisualTransform();
  }

  ProcessAttackHits();
  UpdateAttackEffects(YoRigine::GameTime::GetDeltaTime(YoRigine::TimeChannel::Vfx));

  if (obbCollider_) {
    obbCollider_->Update();
    obbCollider_->SetVelocity(currentVelocity_);
  }
  healthBarUI_->Update();
}

bool BossEnemy::CheckPhaseTransition() {
  const BossState state = GetState();
  if (state == BossState::Dead || state == BossState::PhaseShift ||
      state == BossState::Intro) {
    return false;
  }

  const float thresholds[] = {params_.phase2HpRatio, params_.phase3HpRatio};
  if (phase_ >= static_cast<int>(std::size(thresholds)))
    return false;

  const float threshold = thresholds[phase_];
  if (threshold <= 0.0f || GetHPRatio() > threshold)
    return false;

  ++phase_;
  Logger(("[BossEnemy] フェーズ " + std::to_string(phase_ + 1) + " へ移行\n").c_str());
  ChangeState(BossState::PhaseShift);
  return true;
}

/*==========================================================================
攻撃の選択
//========================================================================*/
const EnemyAttack *BossEnemy::PickAttack() const {
  // 候補のIDを作る。Picker は「ID一覧が空なら全攻撃」と解釈するので、
  // 空のまま渡すとクールダウン中の技や雑魚用の技まで混ざる。必ず中身を入れて渡す。
  std::vector<std::string> ids;
  const auto &all = EnemyAttackLibrary::GetInstance().GetAll();
  for (const auto &attack : all) {
    const bool listed =
        enemyData_.attackIds.empty()
            ? attack.RequiresSkeleton() // 未指定なら骨格アニメ前提の技（＝ボス用）だけ
            : std::find(enemyData_.attackIds.begin(), enemyData_.attackIds.end(),
                        attack.id) != enemyData_.attackIds.end();
    if (!listed)
      continue;

    auto it = attackCooldowns_.find(attack.id);
    if (it != attackCooldowns_.end() && it->second > 0.0f)
      continue;

    ids.push_back(attack.id);
  }
  if (ids.empty())
    return nullptr;

  const EnemyAIContext ctx = EnemyAIContext::Capture(
      *this, enemyData_.perception.facingHalfAngleDeg);
  return EnemyAttackPicker::Pick(*this, ids, ctx, enemyData_.perception);
}

const EnemyAttack *BossEnemy::TakePendingAttack() {
  const EnemyAttack *attack = pendingAttack_;
  pendingAttack_ = nullptr;
  return attack;
}

void BossEnemy::OnAttackFinished(const EnemyAttack *attack) {
  if (attack && attack->cooldown > 0.0f) {
    attackCooldowns_[attack->id] = attack->cooldown;
  }
}

void BossEnemy::SetActiveAttack(AttackPlayer *player,
                                const EnemyAttack *attack) {
  activeAttackPlayer_ = player;
  currentAttack_ = attack;
  lastDealtWindow_ = -1; // 攻撃ごとに判定番号を数え直す
  prevHitboxCenters_.clear();
  swingBurstFired_.clear();
  groundImpactFired_.clear();
  lastEffectAttack_ = attack;
  lastEffectTime_ = -1.0f;
  // 中断（怯み・パリィ）で抜けた時は、出ている軌跡をその場で切る
  if (!player && attackEffects_) {
    attackEffects_->StopTrails();
  }

  // 攻撃中だけアニメの前進を本体の移動として取り出す
  SetRootMotionEnabled(player != nullptr);
}

/*==========================================================================
攻撃判定
//========================================================================*/
void BossEnemy::ProcessAttackHits() {
  debugHitboxCenters_.clear();
  debugHitboxRadii_.clear();

  // エディタのプレビュー中は、判定の球を表示するだけでダメージは与えない
  const bool previewOnly = !activeAttackPlayer_ && previewAttackPlayer_;
  const AttackPlayer *source = activeAttackPlayer_ ? activeAttackPlayer_ : previewAttackPlayer_;
  if (!source)
    return;
  const EnemyAttack *attack = activeAttackPlayer_ ? currentAttack_ : source->GetAttack();
  if (!attack)
    return;

  // 判定の大きさはエンカウントのスケールにだけ合わせる。
  // モデル倍率は「素材の大きさの補正」なので、判定はメートルで作ったままにする。
  const float bodyScale = EncounterScale();
  // 巨体の攻撃が届く高さ。0 なら見た目どおりのカプセルで調べる
  const float reachTop =
      params_.hitReachHeight > 0.0f ? params_.hitReachHeight * bodyScale : 0.0f;

  source->CollectActiveHitboxes(activeHitboxes_);
  for (const AttackModifier *hitbox : activeHitboxes_) {
    Vector3 center;
    if (!ResolveHitboxCenter(*hitbox, center))
      continue;
    const float radius = hitbox->radius * bodyScale;

    debugHitboxCenters_.push_back(center);
    debugHitboxRadii_.push_back(radius);

    // 前フレームの中心から今の中心までを通り道とする（初回は今の位置だけ）
    Vector3 from = center;
    if (auto it = prevHitboxCenters_.find(hitbox); it != prevHitboxCenters_.end()) {
      from = it->second;
    }
    prevHitboxCenters_[hitbox] = center;

    if (previewOnly)
      continue;

    // 同じ番号の判定は1回しか当てない（両手が同時に当たっても1ヒット）
    if (hitbox->damageWindow == lastDealtWindow_)
      continue;
    if (!SweptOverlapsPlayer(from, center, radius, reachTop))
      continue;

    lastDealtWindow_ = hitbox->damageWindow;
    DealDamageToPlayer(hitbox->damageMultiplier, attack->parriable);

    // パリィで怯むと Attack 状態を抜けて activeAttackPlayer_ が消える
    if (!activeAttackPlayer_)
      break;
  }
}

bool BossEnemy::ResolveHitboxCenter(const AttackModifier &hitbox,
                                    Vector3 &out) {
  const float bodyScale = EncounterScale();
  const Vector3 offset = hitbox.jointOffset * bodyScale;

  if (hitbox.IsJointHitbox()) {
    return GetJointWorldPosition(hitbox.joint, offset, out);
  }

  // ジョイント指定なし: 本体の腰の高さを基準に、向きに合わせてずらす
  const float yaw = wt_.rotate_.y;
  const Vector3 forward = {std::sin(yaw), 0.0f, std::cos(yaw)};
  const Vector3 right = {std::cos(yaw), 0.0f, -std::sin(yaw)};
  out = wt_.translate_ + Vector3{0.0f, 1.0f * bodyScale, 0.0f} +
        right * offset.x + Vector3{0.0f, offset.y, 0.0f} + forward * offset.z;
  return true;
}

bool BossEnemy::OverlapsPlayer(const Vector3 &center, float radius,
                               float reachTop) const {
  if (!player_)
    return false;

  // プレイヤーを足元から頭までの縦線分＋半径（カプセル）とみなし、
  // 線分上の最も近い点との距離で判定する。
  const Vector3 feet = player_->GetWT().translate_;
  const float bottom = feet.y + params_.playerHitRadius;
  float top =
      feet.y + std::max(params_.playerHitHeight - params_.playerHitRadius, 0.0f);
  // 巨体の攻撃は、頭上を通っても届く高さまでカプセルを伸ばして調べる
  if (reachTop > 0.0f) {
    top = std::max(top, feet.y + reachTop);
  }
  const Vector3 closest = {feet.x, std::clamp(center.y, bottom, top), feet.z};

  const float reach = radius + params_.playerHitRadius;
  return LengthSquared(center - closest) <= reach * reach;
}

bool BossEnemy::SweptOverlapsPlayer(const Vector3 &from, const Vector3 &to,
                                    float radius, float reachTop) const {
  // 半径の半分ずつ刻めば、刻みの間に隙間はできない。
  // 1フレームで極端に飛んだ時（ロード明けなど）に回数が膨らまないよう上限を置く
  const float distance = Length(to - from);
  const float step = std::max(radius * 0.5f, 0.1f);
  const int count =
      std::clamp(static_cast<int>(std::ceil(distance / step)), 1, 16);
  for (int i = 0; i <= count; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(count);
    if (OverlapsPlayer(Lerp(from, to, t), radius, reachTop)) {
      return true;
    }
  }
  return false;
}

void BossEnemy::DealDamageToPlayer(float multiplier, bool parriable) {
  if (!player_ || player_->IsInvincible())
    return;

  const int damage = std::max(
      1, static_cast<int>(std::lround(enemyData_.attack * multiplier)));
  const auto result = player_->ApplyDamage(damage, wt_.translate_);

  // ── 命中の演出 ──
  // 胸の高さに閃光と火花。ガードされた時は控えめにして、素通しとの差を見せる
  const bool guarded = result != PlayerGuard::GuardResult::GuardFail;
  if (attackEffects_) {
    const Vector3 chest = player_->GetWT().translate_ +
                          Vector3{0.0f, params_.playerHitHeight * 0.6f, 0.0f};
    attackEffects_->PlayHitBurst(chest, guarded);
  }
  if (auto *camera = player_->GetPlayerCamera();
      camera && params_.hitShake > 0.0f) {
    camera->StartShake(params_.hitShake * (guarded ? 0.5f : 1.0f), 0.35f);
  }

  // パリィされたら、その技が崩せる設定のときだけ大きく怯む。
  // スーパーアーマー中はパリィでも止まらない（大技を受け止めさせる設計にする時用）
  if (result == PlayerGuard::GuardResult::ParrySuccess && parriable &&
      !superArmor_) {
    pendingStaggerDuration_ = params_.parryStaggerDuration;
    poise_ = 0.0f;
    ChangeState(BossState::Stagger);
  }
}

/*==========================================================================
被弾
//========================================================================*/
/*==========================================================================
攻撃の演出
//========================================================================*/
void BossEnemy::UpdateAttackEffects(float deltaTime) {
  if (!attackEffects_) {
    return;
  }

  // 実戦の攻撃を優先し、無ければエディタのプレビューを見る（見た目の確認用）
  const AttackPlayer *source =
      activeAttackPlayer_ ? activeAttackPlayer_ : previewAttackPlayer_;
  const EnemyAttack *attack =
      activeAttackPlayer_ ? currentAttack_ : (source ? source->GetAttack() : nullptr);
  const bool playing = source && attack && source->IsPlaying();
  const float time = playing ? source->GetTime() : -1.0f;
  const bool live = activeAttackPlayer_ != nullptr; // プレビューは見た目だけ

  // 攻撃が変わった・プレビューで時間が巻き戻ったら、「1回だけ」の記録を数え直す
  if (attack != lastEffectAttack_ || time < lastEffectTime_) {
    swingBurstFired_.clear();
    groundImpactFired_.clear();
    lastEffectAttack_ = attack;
  }
  lastEffectTime_ = time;

  //------------------------------------------------------------
  // 手の軌跡：判定が出る少し前から終わるまで、その手に軌跡を引く
  //------------------------------------------------------------
  constexpr size_t kLeft = static_cast<size_t>(BossAttackEffects::Hand::Left);
  constexpr size_t kRight = static_cast<size_t>(BossAttackEffects::Hand::Right);
  std::array<std::string, 2> trailJoint = {"LeftHand", "RightHand"};
  std::array<bool, 2> trailActive = {false, false};
  if (playing) {
    for (const auto &modifier : attack->modifiers) {
      if (!modifier.IsJointHitbox())
        continue;
      if (time < modifier.startTime - kTrailLeadTime || time > modifier.endTime)
        continue;
      // 名前に Left を含む手は左の軌跡、それ以外（右手・武器など）は右の軌跡に乗せる
      const size_t side =
          modifier.joint.find("Left") != std::string::npos ? kLeft : kRight;
      trailJoint[side] = modifier.joint;
      trailActive[side] = true;
    }
  }
  for (size_t side : {kLeft, kRight}) {
    Vector3 tip;
    if (!GetJointWorldPosition(trailJoint[side], {}, tip))
      continue;
    // 前腕から手先までを帯の幅にする（巨体なので腕の長さがそのまま迫力になる）
    Vector3 root;
    if (!GetJointWorldPosition(ForeArmJointOf(trailJoint[side]), {}, root)) {
      root = Lerp(tip, wt_.translate_ + Vector3{0.0f, standingHeight_ * 0.5f, 0.0f}, 0.15f);
    }
    attackEffects_->UpdateHandTrail(static_cast<BossAttackEffects::Hand>(side),
                                    tip, root, trailActive[side]);
  }
  attackEffects_->Update(deltaTime);

  if (!playing) {
    return;
  }

  //------------------------------------------------------------
  // 一発もの：拳圧・地面への衝撃・広範囲の衝撃波
  //------------------------------------------------------------
  auto fired = [](const std::vector<const AttackModifier *> &list,
                  const AttackModifier *modifier) {
    return std::find(list.begin(), list.end(), modifier) != list.end();
  };
  const float bodyScale = EncounterScale();
  const float groundY = GroundHeight();

  for (const auto &modifier : attack->modifiers) {
    if (modifier.type != AttackModifierType::Hitbox)
      continue;

    Vector3 center;
    if (modifier.IsJointHitbox()) {
      // 拳圧：判定が出た瞬間に手元で1回
      if (modifier.IsActiveAt(time) && !fired(swingBurstFired_, &modifier) &&
          ResolveHitboxCenter(modifier, center)) {
        swingBurstFired_.push_back(&modifier);
        attackEffects_->PlaySwingBurst(center);
      }

      // 叩きつけ：判定の範囲（＋猶予）の間に、手が地面近くまで降りたら1回。
      // タイミングをデータに書かなくても、アニメの動きから自動で拾える
      const bool inWindow =
          time >= modifier.startTime && time <= modifier.endTime + kImpactGraceTime;
      if (inWindow && !fired(groundImpactFired_, &modifier) &&
          ResolveHitboxCenter(modifier, center) &&
          center.y - groundY <= params_.groundImpactHeight * bodyScale) {
        groundImpactFired_.push_back(&modifier);
        TriggerGroundImpact({center.x, groundY, center.z}, &modifier, live);
      }
      continue;
    }

    // 本体の判定（着地・咆哮）：出た瞬間に足元で1回。
    // ダメージは判定そのものが持つので、ここは見た目と揺れだけ
    if (modifier.IsActiveAt(time) && !fired(groundImpactFired_, &modifier) &&
        ResolveHitboxCenter(modifier, center)) {
      groundImpactFired_.push_back(&modifier);
      const Vector3 ground = {center.x, groundY, center.z};
      const float radius = modifier.radius * bodyScale;
      if (radius >= params_.blastRadiusThreshold * bodyScale) {
        TriggerShockBlast(ground, radius, live);
      } else {
        TriggerGroundImpact(ground, nullptr, live);
      }
    }
  }
}

void BossEnemy::TriggerGroundImpact(const Vector3 &groundPosition,
                                    const AttackModifier *damageSource,
                                    bool live) {
  attackEffects_->PlayGroundImpact(groundPosition,
                                   params_.impactEffectScale * EncounterScale());
  if (!live) {
    return;
  }
  ShakeByDistance(groundPosition, params_.impactShake);

  // 衝撃波の範囲ダメージ。地面にいるプレイヤーを水平距離で調べる。
  // 直撃と同じ判定番号を使うので、直撃した後に衝撃波でもう一度当たることはない
  if (!damageSource || !player_ ||
      damageSource->damageWindow == lastDealtWindow_) {
    return;
  }
  Vector3 toPlayer = player_->GetWT().translate_ - groundPosition;
  toPlayer.y = 0.0f;
  const float reach =
      params_.impactDamageRadius * EncounterScale() + params_.playerHitRadius;
  if (LengthSquared(toPlayer) > reach * reach) {
    return;
  }
  lastDealtWindow_ = damageSource->damageWindow;
  // 地面を伝う衝撃はパリィで弾けない（ガードは効く）
  DealDamageToPlayer(params_.impactDamageMultiplier * damageSource->damageMultiplier,
                     false);
}

void BossEnemy::TriggerShockBlast(const Vector3 &groundPosition, float radius,
                                  bool live) {
  attackEffects_->PlayShockBlast(groundPosition, radius);
  if (live) {
    // 広範囲の衝撃は、範囲の外にいても少し揺れが届くよう強めにする
    ShakeByDistance(groundPosition, params_.impactShake * 1.3f);
  }
}

void BossEnemy::ShakeByDistance(const Vector3 &source, float strength) {
  if (!player_ || strength <= 0.0f) {
    return;
  }
  Vector3 toPlayer = player_->GetWT().translate_ - source;
  toPlayer.y = 0.0f;
  const float range = std::max(params_.impactShakeRange, 0.01f);
  // 近いほど強く。距離 0 で 1、range で 0
  const float nearness = 1.0f - std::clamp(Length(toPlayer) / range, 0.0f, 1.0f);
  if (nearness <= 0.0f) {
    return;
  }

  if (auto *camera = player_->GetPlayerCamera()) {
    camera->StartShake(strength * nearness, 0.25f + 0.3f * nearness);
  }
  // 低音側（左モーター）を強めに回すと「重い地響き」になる
  const auto low = static_cast<uint16_t>(52000.0f * nearness);
  const auto high = static_cast<uint16_t>(26000.0f * nearness);
  YoRigine::Input::GetInstance()->StartVibration(0, 0.2f + 0.25f * nearness, low,
                                                 high);
}

std::string BossEnemy::ForeArmJointOf(const std::string &handJoint) {
  // Mixamo 系の命名（LeftHand / LeftForeArm）に合わせる。
  // 該当しなければ空を返し、呼び出し側が体の中心寄りの点で代用する
  const auto pos = handJoint.rfind("Hand");
  if (pos == std::string::npos) {
    return {};
  }
  return handJoint.substr(0, pos) + "ForeArm";
}

float BossEnemy::GroundHeight() const {
  return player_ ? player_->GetWT().translate_.y : wt_.translate_.y;
}

void BossEnemy::OnEnterCollision([[maybe_unused]] BaseCollider *self,
                                 BaseCollider *other) {
  if (!isAlive_ || isInvincible_ || !player_)
    return;

  // 剣が当たった時の受け口は雑魚と同じ。被弾後の判断は OnDamaged に寄せる
  if (other->GetTypeID() ==
      static_cast<uint32_t>(CollisionTypeIdDef::kPlayerWeapon)) {
    auto *combo = player_->GetCombat()->GetCombo();

    DamageInfo info{};
    info.amount = static_cast<int>(combo->GetCurrentDamage());
    info.sourcePosition = player_->GetWorldPosition();
    ApplyDamage(info);

    combo->OnHitStep(wt_.translate_);
  }
}

void BossEnemy::OnCollision([[maybe_unused]] BaseCollider *self,
                            [[maybe_unused]] BaseCollider *other) {
  // ボスの攻撃は ProcessAttackHits が球で判定するので、体の接触では何もしない。
  // 雑魚同士の押し合いも、ボスは押される側にならない。
}

void BossEnemy::OnDamaged(const DamageInfo &info) {
  if (player_) {
    const bool isSine = player_->GetCombat()->GetComboDamageMultiplier() > 1.0f;
    DamageNumberManager::GetInstance()->SpawnDamage(info.amount, wt_.translate_,
                                                    isSine);
  }

  // 見た目だけ小さくのけぞる。ボスはノックバックで押されない
  Vector3 dir = wt_.translate_ - info.sourcePosition;
  dir.y = 0.0f;
  if (Length(dir) > 0.001f) {
    StartDirectionalHitReaction(Normalize(dir),
                                enemyData_.damageReaction.hitReactionAngle * 0.5f,
                                enemyData_.damageReaction.hitReactionDuration);
  }

  // 怯み値を溜める。上限を超えたら崩れる（スーパーアーマー中は溜まるだけ）
  const BossState state = GetState();
  if (state == BossState::Stagger || state == BossState::Dead ||
      state == BossState::PhaseShift || state == BossState::Intro) {
    return;
  }
  poise_ += static_cast<float>(info.amount);
  if (poise_ >= params_.poiseMax && !superArmor_) {
    poise_ = 0.0f;
    pendingStaggerDuration_ = params_.staggerDuration;
    ChangeState(BossState::Stagger);
  }
}

/*==========================================================================
カメラ用の情報
//========================================================================*/
bool BossEnemy::GetCameraFraming(Vector3 &focus, float &height) {
  const Vector3 feet = wt_.translate_;

  // 頭のジョイントで今の高さを測る。見つからなければスケールから概算する
  Vector3 head{};
  float measured = 0.0f;
  if (GetJointWorldPosition(params_.cameraTopJoint, {}, head)) {
    measured = head.y - feet.y;
  } else {
    measured = wt_.scale_.y;
    head = feet + Vector3{0.0f, measured, 0.0f};
  }

  // カメラの距離に使う身長は「立っている時の高さ」に固定する。
  // 攻撃中（跳躍で頭が倍の高さへ上がる・しゃがむ）まで測り直すと、
  // 技を出すたびにカメラが寄ったり引いたりしてしまう。
  // 待機・追跡・怯みの間だけ測り、ゆっくり均す。
  const BossState state = GetState();
  const bool standing = state == BossState::Idle || state == BossState::Chase ||
                        state == BossState::Stagger;
  // アニメ適用前（バトル開始演出中）は関節が原点に潰れているので測らない
  const bool valid = measured > 0.5f;
  if (valid && standingHeight_ <= 0.0f) {
    standingHeight_ = measured; // 最初の1回はそのまま採用
  } else if (valid && standing) {
    standingHeight_ += (measured - standingHeight_) * 0.02f;
  }
  if (standingHeight_ <= 0.0f)
    return false; // まだ測れていない

  height = standingHeight_;

  // 胸のジョイントが無ければ、足元と頭の間（7割の高さ）で代用する
  if (!GetJointWorldPosition(params_.cameraFocusJoint, {}, focus)) {
    focus = feet + (head - feet) * 0.7f;
  }
  return true;
}

/*==========================================================================
エディタ用
//========================================================================*/
void BossEnemy::SaveTuning() {
  if (jsonManager_) {
    jsonManager_->Save();
  }
}

void BossEnemy::ApplyModelScale(float modelScale) {
  const float encounter = EncounterScale();
  params_.modelScale = std::max(modelScale, 0.01f);
  const float s = encounter * params_.modelScale;
  wt_.scale_ = {s, s, s};
  if (animation_) {
    animation_->SetBaseScale(wt_.scale_);
  }
  standingHeight_ = 0.0f; // 身長はカメラ用に測り直す
}

void BossEnemy::ForceStagger() {
  if (GetState() == BossState::Dead)
    return;
  pendingStaggerDuration_ = params_.staggerDuration;
  poise_ = 0.0f;
  ChangeState(BossState::Stagger);
}

void BossEnemy::ForcePhaseShift() {
  if (GetState() == BossState::Dead)
    return;
  ++phase_;
  ChangeState(BossState::PhaseShift);
}

void BossEnemy::ForceAttack(const EnemyAttack *attack) {
  if (!attack || GetState() == BossState::Dead)
    return;
  aiPaused_ = false;
  SetPendingAttack(attack);
  ChangeState(BossState::Attack);
}

std::vector<std::string> BossEnemy::GetJointNames() {
  std::vector<std::string> names;
  if (!HasSkeletalMotion())
    return names;
  for (const auto &[name, index] : obj_->GetModel()->GetSkeleton()->GetJointMap()) {
    names.push_back(name);
  }
  return names;
}

const std::vector<std::string> &BossEnemy::GetAnimationClipNames() {
  if (clipNamesLoaded_)
    return clipNames_;
  clipNamesLoaded_ = true;

  // エンジンはクリップ名の一覧を持たないので、glTF の "animations" を直接読む。
  // 置き場所は Object3d::SetModel と同じ規則（Resources/Models/<ベース名>/<ファイル名>）。
  auto [basePath, fileName] =
      ModelManager::GetInstance()->ParseModelPath(enemyData_.modelPath);
  const std::string path = "Resources/Models/" + basePath + "/" + fileName;
  std::ifstream ifs(path);
  if (!ifs.is_open())
    return clipNames_;
  try {
    const nlohmann::json j = nlohmann::json::parse(ifs);
    if (j.contains("animations")) {
      for (const auto &anim : j["animations"]) {
        clipNames_.push_back(anim.value("name", std::string()));
      }
    }
  } catch (const std::exception &) {
    clipNames_.clear();
  }
  return clipNames_;
}

/*==========================================================================
出現演出
//========================================================================*/
void BossEnemy::BeginEmergence(float duration) {
  if (!obj_ || duration <= 0.0f)
    return;
  emergenceDuration_ = duration;
  emergenceTimer_ = 0.0f;

  // 死亡時のディゾルブを逆向きに使う。閾値 1（完全に消えた状態）から 0 へ
  obj_->SetDissolveEnabled(true);
  obj_->SetDissolveThreshold(1.0f);
  obj_->SetDissolveEdgeWidth(dissolveEdgeWidth_);
  obj_->SetDissolveEdgeColor(emergenceEdgeColor_);
  obj_->SetDissolveNoiseScale(dissolveNoiseScale_);
}

void BossEnemy::UpdateEmergence(float dt) {
  if (!IsEmerging() || !obj_)
    return;

  emergenceTimer_ += dt;
  const float t = std::min(emergenceTimer_ / emergenceDuration_, 1.0f);
  // 出始めはゆっくり、形が見えてからは一気に（EaseIn）
  obj_->SetDissolveThreshold(1.0f - t * t);

  if (t >= 1.0f) {
    obj_->SetDissolveEnabled(false);
    obj_->SetDissolveThreshold(0.0f);
  }
}

/*==========================================================================
死亡演出
//========================================================================*/
void BossEnemy::StartDissolve() {
  if (!obj_)
    return;
  obj_->SetDissolveEnabled(true);
  obj_->SetDissolveThreshold(0.0f);
  obj_->SetDissolveEdgeWidth(dissolveEdgeWidth_);
  obj_->SetDissolveEdgeColor(dissolveEdgeColor_);
  obj_->SetDissolveNoiseScale(dissolveNoiseScale_);
}

void BossEnemy::SetDissolveProgress(float t) {
  if (obj_)
    obj_->SetDissolveThreshold(t);
}

/*==========================================================================
描画
//========================================================================*/
void BossEnemy::Draw() {
  if (obj_)
    obj_->Draw(camera_, visualWt_);
}

void BossEnemy::DrawVfx() {
  if (attackEffects_) {
    attackEffects_->Draw();
  }
}

void BossEnemy::DrawShadow() {
  if (obj_)
    obj_->DrawShadow(visualWt_);
}

void BossEnemy::DrawUI() {
  if (isAlive_ && player_ && player_->IsAlive()) {
    healthBarUI_->Draw();
  }
}

void BossEnemy::DrawCollision() {
  if (obbCollider_)
    obbCollider_->Draw();

#ifdef USE_IMGUI
  // 攻撃判定の球。アニメの腕と判定がずれていないかを目で確かめるためのもの
  if (debugLine_ && !debugHitboxCenters_.empty()) {
    debugLine_->Reset();
    debugLine_->SetColor({1.0f, 0.25f, 0.2f, 1.0f});
    for (size_t i = 0; i < debugHitboxCenters_.size(); ++i) {
      debugLine_->DrawSphere(debugHitboxCenters_[i], debugHitboxRadii_[i], 16);
    }
    debugLine_->DrawLine();
  }
#endif
}

const char *BossEnemy::GetStateName() const {
  return BossStateToString(fsm_.GetCurrentState());
}
