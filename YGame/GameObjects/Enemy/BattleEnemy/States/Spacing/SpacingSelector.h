#pragma once
#include "../../BattleEnemy.h"
#include "../Attack/AttackSelector.h"
#include "../BattleApproachState.h"
#include "BattleBackstepState.h"
#include "BattleObserveState.h"
#include "BattleStrafeState.h"

#include <memory>
#include <random>

/// <summary>
/// 攻撃と攻撃の「間」に何をするかを決めるヘルパー。
///
/// 従来は攻撃が終わると必ず
/// BattleIdleState（半径数mのランダム徘徊）へ落ちていたため、
/// 「攻めてくる」ではなく「気まぐれに突っ込んでくる」ように見えていた。
/// ここで後退／横移動／様子見を挟むことで戦闘にリズムを作る。
///
/// 無限に間合いを取り続けないよう、遷移は必ず
///   攻撃 → 間合い取り → 再交戦（接近 or 攻撃）
/// の一方向になるようにしている。
/// </summary>
class SpacingSelector {
public:
  /// <summary>
  /// 攻撃・ダウンなどが終わった直後の行動を選ぶ（後退／横移動／様子見）
  /// </summary>
  static std::unique_ptr<IEnemyState<BattleEnemy>>
  SelectAfterAttack(const BattleEnemy &enemy) {
    const BattleEnemyData &data = enemy.GetEnemyData();
    const SpacingParams &params = data.spacing;
    const PerceptionParams &perception = data.perception;

    // プレイヤーがいないなら間合いを測る意味がないので、その場で様子を見る
    if (!enemy.GetPlayer()) {
      return std::make_unique<BattleObserveState>();
    }

    const EnemyAIContext ctx =
        EnemyAIContext::Capture(enemy, perception.facingHalfAngleDeg);

    // 相手が隙を晒しているなら間合いを取らずに差し込む。
    // ここで一拍置いてしまうと、せっかくの隙を毎回見逃すことになる。
    if (ctx.HasOpening(perception) &&
        ctx.distance <
            data.attackStateRange + perception.openingAttackRangeBonus) {
      return AttackSelector::SelectSmartAttack(enemy);
    }

    // 近すぎるときは迷わず後退する。密着状態が続くのが一番「戦っている感じ」を殺す
    if (ctx.distance < params.tooCloseDistance) {
      return std::make_unique<BattleBackstepState>();
    }

    // 基本の重みに、プレイヤーの状態による補正を掛ける
    float backstepWeight = params.backstepWeight;
    float strafeWeight = params.strafeWeight;
    const float observeWeight = params.observeWeight;
    if (perception.enabled) {
      // こちらを向いて振ってきているので下がって空振りさせる
      if (ctx.IsThreatening()) {
        backstepWeight *= perception.threatBackstepWeight;
      }
      // 正面は固いので側面へ回り込む。
      // ここで Strafe を返しても、その後は必ず再交戦へ抜けるのでループしない。
      if (ctx.playerIsGuarding) {
        strafeWeight *= perception.guardStrafeWeight;
      }
    }

    const float total = backstepWeight + strafeWeight + observeWeight;
    if (total <= 0.0f) {
      return std::make_unique<BattleStrafeState>();
    }

    float roll = RandomRange(0.0f, total);
    if ((roll -= backstepWeight) < 0.0f)
      return std::make_unique<BattleBackstepState>();
    if ((roll -= strafeWeight) < 0.0f)
      return std::make_unique<BattleStrafeState>();
    return std::make_unique<BattleObserveState>();
  }

  /// <summary>
  /// 後退が終わった後の行動を選ぶ（横移動 or 様子見）。
  ///
  /// 突進系の攻撃はプレイヤーに密着した状態で終わるため、SelectAfterAttack が
  /// ほぼ毎回「近すぎ→後退」に倒れる。そこから常に様子見へ繋ぐと、敵は
  /// 少し下がって突っ立つだけになり、攻撃のクールダウンと見分けが付かない。
  /// 後退で間合いが戻った後は横移動を選べるようにして、動きを見えるようにする。
  /// </summary>
  static std::unique_ptr<IEnemyState<BattleEnemy>>
  SelectAfterBackstep(const BattleEnemy &enemy) {
    const SpacingParams &params = enemy.GetEnemyData().spacing;

    if (!enemy.GetPlayer()) {
      return std::make_unique<BattleObserveState>();
    }

    const float total = params.strafeWeight + params.observeWeight;
    if (total <= 0.0f) {
      return std::make_unique<BattleStrafeState>();
    }

    float roll = RandomRange(0.0f, total);
    if ((roll -= params.strafeWeight) < 0.0f)
      return std::make_unique<BattleStrafeState>();
    return std::make_unique<BattleObserveState>();
  }

  /// <summary>
  /// 間合い取りが終わった後、交戦へ戻るための行動を選ぶ（接近 or 攻撃）
  /// </summary>
  static std::unique_ptr<IEnemyState<BattleEnemy>>
  SelectReengage(const BattleEnemy &enemy) {
    if (!enemy.GetPlayer()) {
      return std::make_unique<BattleObserveState>();
    }

    const BattleEnemyData &data = enemy.GetEnemyData();
    const EnemyAIContext ctx =
        EnemyAIContext::Capture(enemy, data.perception.facingHalfAngleDeg);

    // 相手に隙があるなら、多少遠くても踏み込む
    float attackRange = data.attackStateRange;
    if (ctx.HasOpening(data.perception)) {
      attackRange += data.perception.openingAttackRangeBonus;
    }

    // 攻撃レンジに入っていれば攻撃、遠ければ詰める
    if (ctx.distance < attackRange) {
      return AttackSelector::SelectSmartAttack(enemy);
    }
    return std::make_unique<BattleApproachState>();
  }

  /// <summary>
  /// [min, max] の一様乱数
  /// </summary>
  static float RandomRange(float min, float max) {
    if (max <= min)
      return min;
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(min, max);
    return dist(gen);
  }
};
