#pragma once

class BaseEnemy;
class Player;

///************************* 知覚の設定 *************************///
// 敵がプレイヤーの何を見て、何を「隙」とみなすかの設定。
//
// 注意: プレイヤーの攻撃はオートホーミングで敵に吸い付くため、空振りがほぼ
// 発生しない。したがって「攻撃の振り終わり」は隙として扱わない。
// 代わりに、こちらを見ているか・攻撃リソースが残っているか・誰を狙って
// いるかといった、ホーミングでは埋まらない情報を判断材料にする。
struct PerceptionParams {
  // false にすると距離とHPだけで判断する従来挙動に戻る（A/B比較用）
  bool enabled = true;

  // 「こちらを見ている」とみなす角度。プレイヤーの正面からの半角（度）
  float facingHalfAngleDeg = 70.0f;

  // ── 何を隙とみなすか ──
  bool openingOnStagger = true;     // のけぞり・スタン中
  bool openingOnOutOfCC = true;     // CCが尽きて攻撃を出せない
  bool openingOnLookAway = true;    // こちらを見ていない（背後・側面）
  bool openingOnLockedOther = true; // 別の敵にロックオンしている

  // ── 隙を見つけたときの動き ──
  float openingAttackRangeBonus = 3.0f; // 攻撃レンジをどれだけ広げて踏み込むか
  float openingFastAttackWeight = 2.0f; // 発生の速い技を選ぶ倍率
  float openingSlowAttackWeight = 0.3f; // 溜め技を避ける倍率

  // ── 相手の状態への反応 ──
  float threatBackstepWeight = 3.0f; // こちらを向いて攻撃中 → 後退を選ぶ倍率
  float guardStrafeWeight = 3.0f;    // ガード中 → 回り込みを選ぶ倍率
  float baitSlowAttackWeight = 1.5f; // 回避中 → 溜め技で釣る倍率
};

/// <summary>
/// 敵がプレイヤーについて「今この瞬間に観測できること」のスナップショット。
///
/// 判断のたびに Capture() で作り直し、各セレクタへ渡す。毎フレーム作らないのは
/// 判断が状態遷移のタイミングでしか起きないためで、これが結果的に
/// 「敵が反応するまでのわずかな間」にもなっている。
///
/// 段階7で BehaviorTree を導入するとき、この構造体がそのまま Blackboard
/// になる。 文字列キーの汎用マップにしないのは、型安全と補完を効かせるため。
/// </summary>
struct EnemyAIContext {
  const BaseEnemy *self = nullptr;
  const Player *player = nullptr;

  bool hasPlayer = false;
  float distance = 0.0f;
  float selfHpRatio = 1.0f;
  float playerHpRatio = 1.0f;

  // ── 位置関係 ──
  // プレイヤーの正面からこちらまでの角度（度）。0 なら正対、180 なら真後ろ。
  float facingAngleDeg = 0.0f;
  bool playerIsFacingMe = true;

  // ── ロックオン ──
  bool playerLockedOnMe = false;    // 自分が狙われている
  bool playerLockedOnOther = false; // ロックオン中だが対象は別の敵

  // ── 攻撃リソース（CC）──
  float playerCcRatio = 1.0f;
  bool playerIsOutOfCC = false; // 攻撃を出せない状態

  // ── 行動状態 ──
  bool playerIsAttacking = false; // 攻撃モーション中
  bool playerIsWindingUp = false; // 攻撃の予備動作中（判定発生前）
  bool playerIsRecovering =
      false; // 攻撃判定が終わった後（表示用。隙には数えない）
  bool playerIsGuarding = false;
  bool playerIsDodging = false;   // 回避中（攻撃が当たらない）
  bool playerIsStaggered = false; // のけぞり・スタン中
  bool playerIsInvincible = false;
  bool playerIsMoving = false;
  bool playerIsRunning = false;

  /// <summary>
  /// 攻撃を差し込める隙があるか。どれを隙とみなすかは設定で切り替える。
  /// </summary>
  bool HasOpening(const PerceptionParams &params) const {
    if (!params.enabled || !hasPlayer)
      return false;
    if (params.openingOnStagger && playerIsStaggered)
      return true;
    if (params.openingOnOutOfCC && playerIsOutOfCC)
      return true;
    if (params.openingOnLookAway && !playerIsFacingMe)
      return true;
    if (params.openingOnLockedOther && playerLockedOnOther)
      return true;
    return false;
  }

  /// <summary>
  /// 今踏み込むと危険か。
  /// こちらを向いて攻撃を振っているときだけ危険とみなす。
  /// 別の敵へ振っているなら、こちらにとってはむしろ好機。
  /// </summary>
  bool IsThreatening() const {
    return hasPlayer && playerIsAttacking && playerIsFacingMe;
  }

  /// <summary>
  /// 敵の現在状況からコンテキストを作る
  /// </summary>
  /// <param name="facingHalfAngleDeg">正対とみなす半角（度）</param>
  static EnemyAIContext Capture(const BaseEnemy &enemy,
                                float facingHalfAngleDeg = 70.0f);
};
