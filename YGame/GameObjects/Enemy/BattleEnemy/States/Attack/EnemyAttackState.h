#pragma once

#include "../../../Attack/EnemyAttackAction.h"
#include "../../../IEnemyState.h"
#include "../../BattleEnemy.h"

#include <functional>
#include <string>
#include <unordered_map>

// ============================================================
// データで定義された攻撃を実行する唯一の攻撃ステート
//
// Rush / ChargeRush / Spin / Jump / Combo / Counter の6クラスが
// やっていた「フェーズを時間で切り替える」処理をここに集約する。
// どんな攻撃になるかは EnemyAttackAction（JSON）が決める。
// ============================================================
class EnemyAttackState : public IEnemyState<BattleEnemy> {
public:
  // Enter の前に実行する攻撃を渡す。action の寿命は呼び出し側が保証すること。
  void SetAction(const EnemyAttackAction *action) { action_ = action; }
  const EnemyAttackAction *GetAction() const { return action_; }

  void Enter(BattleEnemy &enemy) override;
  void Update(BattleEnemy &enemy, float dt) override;
  void Exit(BattleEnemy &enemy) override;

  const char *GetName() const override { return name_.c_str(); }

  bool IsAttacking() const override { return true; }
  bool IsContactDamageActive() const override {
    return GetContactDamageWindow() >= 0;
  }
  int GetContactDamageWindow() const override;
  bool CanBeParried() const override { return action_ && action_->parriable; }

  // すべてのフェーズを終えたか（BehaviorTree から進行を見るときに使う）
  bool IsFinished() const { return finished_; }

  // ============================================================
  // Scripted フェーズの登録
  //
  // JSONで書けない特殊挙動のための逃げ道。これが無いと
  // 「特殊な攻撃だけ専用クラス」が復活してデータ駆動が骨抜きになる。
  // ============================================================
  using ScriptedPhaseFunc =
      std::function<void(BattleEnemy &, float /*progress*/, float /*dt*/)>;
  static void RegisterScriptedPhase(const std::string &id,
                                    ScriptedPhaseFunc func);

private:
  // フェーズを切り替える（見た目のアニメ開始などの入り処理を行う）
  void BeginPhase(BattleEnemy &enemy, int phaseIndex);

  // 現在のフェーズを取得（無ければ nullptr）
  const AttackPhase *CurrentPhase() const;

  // 種別ごとの毎フレーム処理
  void UpdateAnticipation(BattleEnemy &enemy, const AttackPhase &phase,
                          float progress);
  void UpdateDash(BattleEnemy &enemy, const AttackPhase &phase, float dt);
  void UpdateSpin(BattleEnemy &enemy, const AttackPhase &phase, float progress,
                  float dt);
  void UpdateLeap(BattleEnemy &enemy, const AttackPhase &phase, float progress);

  // 相手の方向を向く／進行方向を相手へ寄せる
  void FaceTarget(BattleEnemy &enemy);

private:
  const EnemyAttackAction *action_ = nullptr;

  int phaseIndex_ = 0;      // 実行中のフェーズ
  float phaseTimer_ = 0.0f; // そのフェーズでの経過秒
  int loopIteration_ = 0;   // 繰り返し区間の何周目か
  bool finished_ = false;

  // フェーズ開始時の状態。予備動作やジャンプの基準に使う。
  Vector3 phaseStartPos_{};
  float phaseStartYaw_ = 0.0f;
  Vector3 leapTargetPos_{};

  // 突進方向。Dash 開始時に決めてホーミングで補正する。
  Vector3 attackDir_{0.0f, 0.0f, 1.0f};

  // 攻撃開始時のY。フェーズで上下させた後に戻すため。
  float baseY_ = 0.0f;

  // デバッグ表示名（"Attack:rush" など）
  std::string name_ = "Attack";
};
