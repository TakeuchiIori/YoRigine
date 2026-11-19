#include "BaseEnemy.h"

#include "Player/Player.h"

#include <cmath>
#include <numbers>

/*==========================================================================
体力
//========================================================================*/
void BaseEnemy::TakeDamage(int damage) {
  if (isInvincible_ || !IsAlive())
    return;
  currentHp_ -= damage;
  if (currentHp_ < 0)
    currentHp_ = 0;
}

void BaseEnemy::ApplyDamage(const DamageInfo &info) {
  // 無敵や死亡でHPが動かなかったときはリアクションも起こさない
  const int before = currentHp_;
  TakeDamage(info.amount);
  if (currentHp_ == before) {
    return;
  }

  OnDamaged(info);
}

void BaseEnemy::Heal(int amount) {
  if (!IsAlive())
    return;
  currentHp_ += amount;
  if (currentHp_ > maxHp_)
    currentHp_ = maxHp_;
}

/*==========================================================================
ターゲット
//========================================================================*/
Vector3 BaseEnemy::GetPlayerPosition() const {
  if (player_) {
    return player_->GetWorldPosition();
  }
  return Vector3(0.0f, 0.0f, 0.0f);
}

float BaseEnemy::GetDistanceToPlayer() const {
  if (!player_)
    return 0.0f;
  return Length(GetPlayerPosition() - wt_.translate_);
}

/*==========================================================================
回転
//========================================================================*/
void BaseEnemy::RotateTowards(float targetAngle, float speed, float dt) {
  float current = wt_.rotate_.y;

  // 差分を [-π, π] に正規化してから最短方向へ回す
  float diff = targetAngle - current;
  while (diff > std::numbers::pi_v<float>)
    diff -= 2.0f * std::numbers::pi_v<float>;
  while (diff < -std::numbers::pi_v<float>)
    diff += 2.0f * std::numbers::pi_v<float>;

  float maxDelta = speed * dt;
  if (std::abs(diff) <= maxDelta) {
    wt_.rotate_.y = targetAngle;
  } else {
    wt_.rotate_.y = current + std::copysign(maxDelta, diff);
  }
}

void BaseEnemy::RotateTowardsPlayer(float speed, float dt) {
  if (!HasPlayer())
    return;

  Vector3 dir = GetPlayerPosition() - wt_.translate_;
  dir.y = 0.0f;
  if (Length(dir) < 0.1f)
    return;

  RotateTowards(std::atan2(dir.x, dir.z), speed, dt);
}

void BaseEnemy::RotateTowardsDirection(const Vector3 &direction, float speed,
                                       float dt) {
  if (Length(direction) < 0.1f)
    return;
  RotateTowards(std::atan2(direction.x, direction.z), speed, dt);
}

void BaseEnemy::FaceDirection(const Vector3 &direction) {
  if (Length(direction) < 0.01f)
    return;
  wt_.rotate_.y = std::atan2(direction.x, direction.z);
}

void BaseEnemy::FacePlayer() {
  if (!HasPlayer())
    return;
  Vector3 dir = GetPlayerPosition() - wt_.translate_;
  dir.y = 0.0f;
  FaceDirection(dir);
}

/*==========================================================================
骨格アニメーション
//========================================================================*/
void BaseEnemy::SetupSkeletalModel(const std::string &modelFile,
                                   const std::string &initialClip) {
  if (!obj_)
    return;
  obj_->SetModel(modelFile, true, initialClip);

  // スケルトンを持たないモデルを渡された場合は骨格アニメ無しとして扱う
  YoRigine::Model *model = obj_->GetModel();
  if (!model || !model->GetSkeleton() || !model->GetMotionSystem()) {
    motionModelFile_.clear();
    return;
  }
  motionModelFile_ = modelFile;
  PlayMotion(initialClip, true);
}

void BaseEnemy::PlayMotion(const std::string &clip, bool loop, float speed) {
  if (!HasSkeletalMotion() || clip.empty())
    return;

  // SetChangeMotion
  // は「同じクリップなら再生モードだけ変える」実装になっている。 その中で時刻が
  // 0 に戻るので、同じ攻撃を連続で出しても先頭から再生される。
  obj_->SetChangeMotion(motionModelFile_,
                        loop ? MotionPlayMode::Loop : MotionPlayMode::Once,
                        clip);
  obj_->SetMotionSpeed(speed);
}

void BaseEnemy::SetMotionTime(float seconds) {
  if (!HasSkeletalMotion())
    return;
  if (auto *motion = obj_->GetModel()->GetMotionSystem()) {
    motion->SetAnimationTime(seconds);
    // スクラブ中は Update で時刻を進めないので、姿勢だけ即座に反映する
    motion->Apply();
  }
}

float BaseEnemy::GetCurrentMotionDuration() {
  if (!HasSkeletalMotion())
    return 0.0f;
  auto *motion = obj_->GetModel()->GetMotionSystem();
  return motion ? motion->GetDuration() : 0.0f;
}

bool BaseEnemy::GetJointWorldPosition(const std::string &jointName,
                                      const Vector3 &localOffset,
                                      Vector3 &out) {
  if (!HasSkeletalMotion())
    return false;

  Skeleton *skeleton = obj_->GetModel()->GetSkeleton();
  auto &jointMap = skeleton->GetJointMap();
  auto it = jointMap.find(jointName);
  if (it == jointMap.end())
    return false;

  // スケルトン空間 → 描画用ワールド。デバッグのボーン描画と同じ合成順なので、
  // 見えているメッシュの腕と判定の位置がずれない。
  const Matrix4x4 jointWorld =
      Multiply(skeleton->GetJoints()[it->second].GetSkeletonSpaceMatrix(),
               visualWt_.matWorld_);

  // 行ベクトル規約なので 0〜2 行目がジョイントの X/Y/Z 軸、3 行目が位置。
  // 軸を正規化してから使うことで、モデルの単位やボーンのスケールに関係なく
  // offset を「メートル」で指定できる。
  auto axis = [&](int row) {
    Vector3 a{jointWorld.m[row][0], jointWorld.m[row][1], jointWorld.m[row][2]};
    const float len = Length(a);
    return len > 1e-6f ? a / len : Vector3{};
  };
  out = Vector3{jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2]} +
        axis(0) * localOffset.x + axis(1) * localOffset.y +
        axis(2) * localOffset.z;
  return true;
}

/*==========================================================================
ルートモーション
//========================================================================*/
void BaseEnemy::SetRootMotionEnabled(bool enabled) {
  // 有効にした瞬間の腰のずれを「ここから先の差分の起点」にする。
  // これが無いと、直前の待機モーションのずれまで移動として拾ってしまう。
  if (enabled && !rootMotionEnabled_) {
    rootMotionResetLast_ = true;
  }
  rootMotionEnabled_ = enabled;
}

Vector3 BaseEnemy::ConsumeRootMotion() {
  if (!HasSkeletalMotion() || rootMotionJoint_.empty())
    return {};

  YoRigine::Model *model = obj_->GetModel();
  Skeleton *skeleton = model->GetSkeleton();
  MotionSystem *motion = model->GetMotionSystem();
  auto it = skeleton->GetJointMap().find(rootMotionJoint_);
  if (it == skeleton->GetJointMap().end())
    return {};

  // 再生し終わったクリップは Apply
  // されず、前フレームに固定した姿勢のまま残っている。
  // ここで差分を取ると「固定位置－前回のずれ」で逆走してしまうので何もしない。
  if (motion->IsFinished())
    return {};

  std::vector<Joint> &joints = skeleton->GetJoints();
  Joint &root = joints[it->second];
  QuaternionTransform pose = root.GetTransform();

  // 最初に見た腰の位置を固定先にする（モデル読み込み直後の待機ポーズ）
  if (!rootMotionAnchorSet_) {
    rootMotionAnchor_ = pose.translate;
    rootMotionAnchorSet_ = true;
  }

  // アニメが置こうとした位置（スケルトン空間）
  const Vector3 sampled = Joint::ExtractJointPosition(root);

  // 腰を基準の水平位置へ戻して、スケルトンとスキンを組み直す
  pose.translate.x = rootMotionAnchor_.x;
  pose.translate.z = rootMotionAnchor_.z;
  root.SetTransform(pose);
  skeleton->Update();
  if (SkinCluster *skin = model->GetSkinCluster()) {
    skin->UpdateMatrixPalette(joints);
  }

  // 固定後の位置との差が「基準からのずれ」。スケルトン空間で比べるので、
  // 親ボーンのスケールやモデルの単位（cm/m）に左右されない。
  Vector3 offset = sampled - Joint::ExtractJointPosition(root);
  offset.y = 0.0f;

  Vector3 step{};
  if (rootMotionEnabled_) {
    if (rootMotionResetLast_) {
      rootMotionLastOffset_ = offset;
      rootMotionResetLast_ = false;
    }
    step = offset - rootMotionLastOffset_;
  }
  rootMotionLastOffset_ = offset;

  // スケルトン空間の移動量を、ジョイント座標と同じ変換でワールドへ
  Vector3 worldStep = TransformNormal(step, visualWt_.matWorld_);
  worldStep.y = 0.0f;
  return worldStep;
}

/*==========================================================================
ノックバック
//========================================================================*/
void BaseEnemy::StartKnockback(const Vector3 &direction, float power,
                               float duration) {
  knockbackData_.isKnockingBack_ = true;
  knockbackData_.knockbackDirection_ = Vector3::Normalize(direction);
  knockbackData_.knockbackPower_ = power;
  knockbackData_.knockbackDuration_ = duration;
  knockbackData_.knockbackTimer_ = 0.0f;
}

void BaseEnemy::UpdateKnockback(float dt) {
  if (!knockbackData_.isKnockingBack_)
    return;
  knockbackData_.knockbackTimer_ += dt;

  // 時間経過でパワーを減衰させる
  float currentPower = knockbackData_.knockbackPower_ *
                       (1.0f - (knockbackData_.knockbackTimer_ /
                                knockbackData_.knockbackDuration_));
  AddTranslate(knockbackData_.knockbackDirection_ * currentPower * dt);

  if (knockbackData_.knockbackTimer_ >= knockbackData_.knockbackDuration_) {
    knockbackData_.isKnockingBack_ = false;
    knockbackData_.knockbackPower_ = 0.0f;
  }
}

/*==========================================================================
攻撃方向に応じたのけぞり
//========================================================================*/
void BaseEnemy::StartDirectionalHitReaction(const Vector3 &direction,
                                            float angle, float duration) {
  hitReactionAngle_ = angle;
  hitReactionDuration_ = duration;
  hitReactionRotation_ = {};

  Vector3 horizontalDirection = direction;
  horizontalDirection.y = 0.0f;
  if (Length(horizontalDirection) < 0.001f || hitReactionDuration_ <= 0.0f) {
    isHitReacting_ = false;
    return;
  }
  horizontalDirection = Normalize(horizontalDirection);

  // 攻撃のワールド方向を、敵から見た前後・左右成分へ変換する
  const float yaw = wt_.rotate_.y;
  const Vector3 forward = {std::sinf(yaw), 0.0f, std::cosf(yaw)};
  const Vector3 right = {std::cosf(yaw), 0.0f, -std::sinf(yaw)};
  const float forwardAmount = Dot(horizontalDirection, forward);
  const float rightAmount = Dot(horizontalDirection, right);

  hitReactionTargetRotation_ = {-forwardAmount * hitReactionAngle_, 0.0f,
                                -rightAmount * hitReactionAngle_};
  hitReactionTimer_ = 0.0f;
  isHitReacting_ = true;
}

void BaseEnemy::UpdateDirectionalHitReaction(float dt) {
  hitReactionRotation_ = {};

  if (!isHitReacting_)
    return;

  hitReactionTimer_ += dt;
  const float progress =
      std::fminf(hitReactionTimer_ / hitReactionDuration_, 1.0f);
  // 素早く最大まで傾き、そのまま滑らかに基準姿勢へ戻る
  const float weight = std::sinf(progress * std::numbers::pi_v<float>);
  hitReactionRotation_ = hitReactionTargetRotation_ * weight;

  if (progress >= 1.0f) {
    isHitReacting_ = false;
  }
}

/*==========================================================================
ダメージ点滅
//========================================================================*/
void BaseEnemy::UpdateBlinking(float dt) {
  if (!isDamageBlinking_)
    return;
  blinkTimer_ += dt;

  // サイン波でα値を周期的に変化させる
  float alpha = 0.65f + 0.35f * std::sin(blinkTimer_ * blinkSpeed_);

  if (obj_) {
    obj_->GetColor() = {1.0f, 0.0f, 0.0f, alpha};
  }
}

/*==========================================================================
状態VFX（燃焼など）の付着・追従・停止
//========================================================================*/
void BaseEnemy::AttachStatusVfx(const std::string &compositeName,
                                float duration) {
  if (compositeName.empty() || duration <= 0.0f || !isAlive_)
    return;

  // 同じVFXの再付着は時間リフレッシュのみ（ループを二重再生しない）
  if (statusVfx_.IsValid() && statusVfxName_ == compositeName) {
    statusVfxTimer_ = duration;
    return;
  }

  // 別のVFXが付いていたら止めてから付け直す
  StopStatusVfx();
  statusVfx_ = EffectHandle::Play(compositeName, wt_.translate_,
                                  /*loop*/ true, /*emitCount*/ -1);
  statusVfxName_ = compositeName;
  statusVfxTimer_ = duration;
}

void BaseEnemy::UpdateStatusVfx(float dt) {
  if (!statusVfx_.IsValid())
    return;

  statusVfxTimer_ -= dt;
  if (statusVfxTimer_ <= 0.0f) {
    StopStatusVfx();
    return;
  }
  // 本体に追従（少し持ち上げて足元ではなく体に重ねる）
  Vector3 pos = wt_.translate_;
  pos.y += 1.0f;
  statusVfx_.SetPosition(pos);
}

void BaseEnemy::StopStatusVfx() {
  if (statusVfx_.IsValid()) {
    statusVfx_.Stop();
  }
  statusVfxName_.clear();
  statusVfxTimer_ = 0.0f;
}

/*==========================================================================
派生から呼ぶ共通更新
//========================================================================*/
void BaseEnemy::UpdateAnimation(float dt) {
  if (!animation_)
    return;

  animation_->Update(dt);

  // スケールアニメーション中はそれを本体へ反映する
  if (animation_->IsScaleAnimating()) {
    wt_.scale_ = animation_->GetCurrentScale();
  }

  // カラーアニメーション中はそれを本体へ反映する
  if (animation_->IsColorAnimating()) {
    if (obj_) {
      obj_->SetMaterialColor(animation_->GetCurrentColor());
    }
  }
}

void BaseEnemy::UpdateReactions(float dt) {
  UpdateKnockback(dt);
  UpdateDirectionalHitReaction(dt);
  UpdateStatusVfx(dt);
}

void BaseEnemy::SyncVisualTransform() {
  visualWt_.scale_ = wt_.scale_;
  visualWt_.rotate_ = wt_.rotate_ + hitReactionRotation_;
  visualWt_.translate_ = wt_.translate_;
  visualWt_.anchorPoint_ = wt_.anchorPoint_;
  visualWt_.useAnchorPoint_ = wt_.useAnchorPoint_;
  visualWt_.UpdateMatrix();
}

void BaseEnemy::UpdateVelocity() {
  currentVelocity_ = wt_.translate_ - previousPosition_;
}
