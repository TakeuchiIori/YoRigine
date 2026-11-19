#pragma once
#include "../Attack/Runtime/AttackPlayer.h"
#include "BossEnemy.h"

// ============================================================
// ボスの各状態
//
// 雑魚敵の IEnemyState は「遷移のたびに make_unique で作り直す」方式だが、
// ボスは StateMachine<BossState> に全状態を最初に登録して使い回す。
// 使い回すので、前回の残骸を持ち越さないよう OnEnter で必ず値を初期化する。
// ============================================================

// 状態の共通部分。持ち主のボスへの参照を持つだけ
class BossStateBase : public IState<BossState> {
public:
  explicit BossStateBase(BossEnemy &boss) : boss_(boss) {}

protected:
  BossEnemy &boss_;
  float timer_ = 0.0f;
};

///************************* 登場 *************************///
class BossIntroState : public BossStateBase {
public:
  using BossStateBase::BossStateBase;
  void OnEnter() override;
  void Update(float dt) override;
  void OnExit() override;
  BossState GetStateType() const override { return BossState::Intro; }
};

///************************* 思考 *************************///
class BossIdleState : public BossStateBase {
public:
  using BossStateBase::BossStateBase;
  void OnEnter() override;
  void Update(float dt) override;
  BossState GetStateType() const override { return BossState::Idle; }

private:
  float thinkTime_ = 0.0f;
};

///************************* 接近 *************************///
class BossChaseState : public BossStateBase {
public:
  using BossStateBase::BossStateBase;
  void OnEnter() override;
  void Update(float dt) override;
  BossState GetStateType() const override { return BossState::Chase; }

private:
  bool running_ = false;
};

///************************* 攻撃 *************************///
// データで定義された攻撃を AttackPlayer で再生するだけ。
// どんな攻撃かは一切知らない。
class BossAttackState : public BossStateBase {
public:
  using BossStateBase::BossStateBase;
  void OnEnter() override;
  void Update(float dt) override;
  void OnExit() override;
  BossState GetStateType() const override { return BossState::Attack; }

  const AttackPlayer &GetAttackPlayer() const { return player_; }

private:
  AttackPlayer player_;
  const EnemyAttack *attack_ = nullptr;
  bool appliedInvincible_ = false;
};

///************************* 怯み *************************///
class BossStaggerState : public BossStateBase {
public:
  using BossStateBase::BossStateBase;
  void OnEnter() override;
  void Update(float dt) override;
  BossState GetStateType() const override { return BossState::Stagger; }

private:
  float duration_ = 0.0f;
};

///************************* フェーズ移行 *************************///
class BossPhaseShiftState : public BossStateBase {
public:
  using BossStateBase::BossStateBase;
  void OnEnter() override;
  void Update(float dt) override;
  void OnExit() override;
  BossState GetStateType() const override { return BossState::PhaseShift; }
};

///************************* 死亡 *************************///
class BossDeadState : public BossStateBase {
public:
  using BossStateBase::BossStateBase;
  void OnEnter() override;
  void Update(float dt) override;
  BossState GetStateType() const override { return BossState::Dead; }

private:
  bool dissolving_ = false;
  float dissolveTimer_ = 0.0f;
};
