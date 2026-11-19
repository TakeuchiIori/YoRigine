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
  obbCollider_->SetMass(1.0f);
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

  source->CollectActiveHitboxes(activeHitboxes_);
  for (const AttackModifier *hitbox : activeHitboxes_) {
    Vector3 center;
    if (!ResolveHitboxCenter(*hitbox, center))
      continue;
    const float radius = hitbox->radius * bodyScale;

    debugHitboxCenters_.push_back(center);
    debugHitboxRadii_.push_back(radius);
    if (previewOnly)
      continue;

    // 同じ番号の判定は1回しか当てない（両手が同時に当たっても1ヒット）
    if (hitbox->damageWindow == lastDealtWindow_)
      continue;
    if (!OverlapsPlayer(center, radius))
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

bool BossEnemy::OverlapsPlayer(const Vector3 &center, float radius) const {
  if (!player_)
    return false;

  // プレイヤーを足元から頭までの縦線分＋半径（カプセル）とみなし、
  // 線分上の最も近い点との距離で判定する。
  const Vector3 feet = player_->GetWT().translate_;
  const float bottom = feet.y + params_.playerHitRadius;
  const float top =
      feet.y + std::max(params_.playerHitHeight - params_.playerHitRadius, 0.0f);
  const Vector3 closest = {feet.x, std::clamp(center.y, bottom, top), feet.z};

  const float reach = radius + params_.playerHitRadius;
  return LengthSquared(center - closest) <= reach * reach;
}

void BossEnemy::DealDamageToPlayer(float multiplier, bool parriable) {
  if (!player_ || player_->IsInvincible())
    return;

  const int damage = std::max(
      1, static_cast<int>(std::lround(enemyData_.attack * multiplier)));
  const auto result = player_->ApplyDamage(damage, wt_.translate_);

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
