#include "BossStates.h"

#include <algorithm>
#include <random>

namespace {

float RandomRange(float min, float max) {
  if (max <= min)
    return min;
  static std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<float> dist(min, max);
  return dist(gen);
}

// プレイヤーへの水平方向
Vector3 FlatDirectionToPlayer(const BossEnemy &boss) {
  Vector3 dir = boss.GetPlayerPosition() - boss.GetTranslate();
  dir.y = 0.0f;
  return dir;
}

} // namespace

/*==========================================================================
登場
//========================================================================*/
void BossIntroState::OnEnter() {
  timer_ = 0.0f;
  boss_.IsInvincible() = true; // 咆哮中に削られると登場演出の意味がない
  boss_.FacePlayer();
  boss_.PlayMotion(boss_.GetParams().introClip, false);
}

void BossIntroState::Update(float dt) {
  timer_ += dt;
  // クリップが見つからなければ長さ 0 になり、すぐ戦闘へ入る
  if (timer_ >= boss_.GetCurrentMotionDuration()) {
    boss_.ChangeState(BossState::Idle);
  }
}

void BossIntroState::OnExit() { boss_.IsInvincible() = false; }

/*==========================================================================
思考
//========================================================================*/
void BossIdleState::OnEnter() {
  timer_ = 0.0f;
  const auto &params = boss_.GetParams();
  thinkTime_ = RandomRange(params.thinkTimeMin, params.thinkTimeMax);
  boss_.PlayMotion(params.idleClip, true);
}

void BossIdleState::Update(float dt) {
  if (!boss_.HasPlayer())
    return;

  timer_ += dt;
  boss_.RotateTowardsPlayer(boss_.GetParams().turnSpeed, dt);

  if (timer_ < thinkTime_)
    return;

  // 届く技があれば出す。無ければ距離を詰める。
  if (const EnemyAttack *attack = boss_.PickAttack()) {
    boss_.SetPendingAttack(attack);
    boss_.ChangeState(BossState::Attack);
  } else {
    boss_.ChangeState(BossState::Chase);
  }
}

/*==========================================================================
接近
//========================================================================*/
void BossChaseState::OnEnter() {
  timer_ = 0.0f;
  running_ = boss_.GetDistanceToPlayer() > boss_.GetParams().runDistance;
  const auto &params = boss_.GetParams();
  boss_.PlayMotion(running_ ? params.runClip : params.walkClip, true);
}

void BossChaseState::Update(float dt) {
  if (!boss_.HasPlayer())
    return;

  const auto &params = boss_.GetParams();
  timer_ += dt;

  // 走り⇔歩きの切り替え。境目で毎フレーム切り替わらないよう 1m の幅を持たせる
  const float distance = boss_.GetDistanceToPlayer();
  const bool shouldRun = running_ ? distance > params.runDistance - 1.0f
                                  : distance > params.runDistance + 1.0f;
  if (shouldRun != running_) {
    running_ = shouldRun;
    boss_.PlayMotion(running_ ? params.runClip : params.walkClip, true);
  }

  // 向きを合わせながら前進する。体が重なるほど近づいたら止まる
  const Vector3 dir = FlatDirectionToPlayer(boss_);
  boss_.RotateTowardsDirection(dir, params.turnSpeed, dt);
  if (Length(dir) > 1.5f) {
    const float speed = boss_.GetEnemyData().moveSpeed *
                        (running_ ? params.runSpeedMultiplier : 1.0f);
    boss_.AddTranslate(Normalize(dir) * speed * dt);
  }

  // 届く技が出たら即座に攻撃へ
  if (const EnemyAttack *attack = boss_.PickAttack()) {
    boss_.SetPendingAttack(attack);
    boss_.ChangeState(BossState::Attack);
    return;
  }

  // どの技の射程にも入らないまま時間が過ぎたら一度考え直す
  if (timer_ >= params.chaseGiveUpTime) {
    boss_.ChangeState(BossState::Idle);
  }
}

/*==========================================================================
攻撃
//========================================================================*/
void BossAttackState::OnEnter() {
  appliedInvincible_ = false;
  attack_ = boss_.TakePendingAttack();
  if (!attack_) {
    return; // Update で Idle へ戻す（OnEnter 内で遷移すると OnExit
            // が二重に走る）
  }

  // 開始時に一度だけ相手を向く。以降の向きはモディファイアが決める
  boss_.FacePlayer();
  player_.Play(boss_, attack_);
  boss_.SetActiveAttack(&player_, attack_);
}

void BossAttackState::Update(float dt) {
  if (!attack_) {
    boss_.ChangeState(BossState::Idle);
    return;
  }

  player_.Update(boss_, dt);

  // 無敵はモディファイアの区間に追従させる。自分が付けた分だけを戻す
  const bool shouldBeInvincible = player_.IsInvincibleNow();
  if (shouldBeInvincible != appliedInvincible_) {
    boss_.IsInvincible() = shouldBeInvincible;
    appliedInvincible_ = shouldBeInvincible;
  }
  boss_.SetSuperArmor(player_.IsSuperArmorNow());

  if (player_.IsFinished()) {
    boss_.ChangeState(BossState::Idle);
  }
}

void BossAttackState::OnExit() {
  if (appliedInvincible_) {
    boss_.IsInvincible() = false;
    appliedInvincible_ = false;
  }
  boss_.SetSuperArmor(false);
  boss_.SetActiveAttack(nullptr, nullptr);

  // 怯みなどで中断されてもクールダウンは開始する（同じ技の連発を防ぐ）
  boss_.OnAttackFinished(attack_);

  // 位置は攻撃結果として残し、回転・スケールだけ戻す
  player_.StopKeepPosition(boss_);
  attack_ = nullptr;
}

/*==========================================================================
怯み
//========================================================================*/
void BossStaggerState::OnEnter() {
  timer_ = 0.0f;
  duration_ = boss_.GetPendingStaggerDuration();
  // 専用の被弾クリップが無いので、待機をゆっくり流して「固まった」感じを出す
  boss_.PlayMotion(boss_.GetParams().idleClip, true, 0.3f);
}

void BossStaggerState::Update(float dt) {
  timer_ += dt;
  if (timer_ >= duration_) {
    boss_.ChangeState(BossState::Idle);
  }
}

/*==========================================================================
フェーズ移行
//========================================================================*/
void BossPhaseShiftState::OnEnter() {
  timer_ = 0.0f;
  boss_.IsInvincible() = true;
  boss_.PlayMotion(boss_.GetParams().phaseShiftClip, false);
}

void BossPhaseShiftState::Update(float dt) {
  timer_ += dt;
  if (timer_ >= boss_.GetCurrentMotionDuration()) {
    boss_.ChangeState(BossState::Idle);
  }
}

void BossPhaseShiftState::OnExit() { boss_.IsInvincible() = false; }

/*==========================================================================
死亡
//========================================================================*/
void BossDeadState::OnEnter() {
  timer_ = 0.0f;
  dissolving_ = false;
  dissolveTimer_ = 0.0f;
  boss_.SetCanAct(false);
  boss_.IsInvincible() = true;
  boss_.StopStatusVfx();
  boss_.PlayMotion(boss_.GetParams().deathClip, false);
}

void BossDeadState::Update(float dt) {
  timer_ += dt;

  // 倒れ切ってから消し始める
  if (!dissolving_) {
    if (timer_ >= boss_.GetCurrentMotionDuration()) {
      dissolving_ = true;
      boss_.StartDissolve();
    }
    return;
  }

  dissolveTimer_ += dt;
  const float duration = boss_.GetDissolveDuration();
  const float t =
      duration > 0.0f ? std::min(dissolveTimer_ / duration, 1.0f) : 1.0f;
  boss_.SetDissolveProgress(t);

  // 生存フラグを落とすとマネージャが撃破として回収する
  if (t >= 1.0f) {
    boss_.SetIsAlive(false);
  }
}
