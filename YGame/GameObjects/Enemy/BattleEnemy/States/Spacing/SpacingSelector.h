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
    const SpacingParams &params = enemy.GetEnemyData().spacing;

    // プレイヤーがいないなら間合いを測る意味がないので、その場で様子を見る
    if (!enemy.GetPlayer()) {
      return std::make_unique<BattleObserveState>();
    }

    // 近すぎるときは迷わず後退する。密着状態が続くのが一番「戦っている感じ」を殺す
    if (enemy.GetDistanceToPlayer() < params.tooCloseDistance) {
      return std::make_unique<BattleBackstepState>();
    }

    // それ以外は重み抽選
    const float total =
        params.backstepWeight + params.strafeWeight + params.observeWeight;
    if (total <= 0.0f) {
      return std::make_unique<BattleStrafeState>();
    }

    float roll = RandomRange(0.0f, total);
    if ((roll -= params.backstepWeight) < 0.0f)
      return std::make_unique<BattleBackstepState>();
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

    // 攻撃レンジに入っていれば攻撃、遠ければ詰める
    if (enemy.GetDistanceToPlayer() < enemy.GetEnemyData().attackStateRange) {
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
