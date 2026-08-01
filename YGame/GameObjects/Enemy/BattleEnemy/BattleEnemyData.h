#pragma once
#include "../AI/EnemyAIContext.h" // PerceptionParams
#include "Vector3.h"
#include <string>
#include <vector>

///************************* 状態定義 *************************///
enum class BattleEnemyState {
  Idle,     // 待機
  Approach, // 接近
  Attack,   // 攻撃
  Damaged,  // 被弾
  Dead      // 撃破
};

///************************* 攻撃パターン定義 *************************///
// JSON上は従来通り文字列で保存する (AttackPatternToString/FromString で変換)。
// enum化する前は "rush"/"chargeRush" 等の文字列比較が各所に散在し、
// "charge" と "chargeRush"
// の表記ゆれのようなタイプミスがコンパイル時に検出できなかった。
enum class AttackPatternType {
  Rush,
  Spin,
  ChargeRush,
  Combo,
  Jump,
  Unknown, // 未知の文字列 (JSONの手打ちミス等) が来た場合のフォールバック
};

inline const char *AttackPatternToString(AttackPatternType type) {
  switch (type) {
  case AttackPatternType::Rush:
    return "rush";
  case AttackPatternType::Spin:
    return "spin";
  case AttackPatternType::ChargeRush:
    return "chargeRush";
  case AttackPatternType::Combo:
    return "combo";
  case AttackPatternType::Jump:
    return "jump";
  default:
    return "unknown";
  }
}

inline AttackPatternType AttackPatternFromString(const std::string &name) {
  if (name == "rush")
    return AttackPatternType::Rush;
  if (name == "spin")
    return AttackPatternType::Spin;
  if (name == "chargeRush")
    return AttackPatternType::ChargeRush;
  if (name == "combo")
    return AttackPatternType::Combo;
  if (name == "jump")
    return AttackPatternType::Jump;
  return AttackPatternType::Unknown;
}

///************************* 攻撃用構造体 *************************///

// 突進攻撃
struct RushAttackParams {
  // 予備動作フェーズ（後ろに引く）
  float anticipationTime = 0.5f;     // 予備動作時間
  float anticipationDistance = 1.5f; // 後退する距離

  float chargeTime = 1.0f;      // 溜め時間
  float rushTime = 0.5f;        // 突進している時間
  float speedMultiplier = 7.0f; // 速度倍率
  float cooldownTime = 1.2f;    // 突進後の硬直
};

// 強力な突進攻撃
struct ChargeRushAttackParams {
  // 予備動作フェーズ（地面を踏み込む）
  float anticipationTime = 0.8f;            // 予備動作時間
  float stompIntensity = 0.4f;              // 踏み込みの沈み込み
  float anticipationColorPulseSpeed = 8.0f; // 色の点滅速度

  float chargeTime = 1.5f;       // 長い溜め
  float rushTime = 0.5f;         // 突進時間
  float speedMultiplier = 12.0f; // より速い突進
  float cooldownTime = 1.0f;
};

struct SpinAttackParams {
  // 予備動作フェーズ（体を捻る）
  float anticipationTime = 0.5f;           // 予備動作時間
  float twistAngle = 1.57f;                // 捻る角度（ラジアン、約90度）
  float anticipationColorIntensity = 0.7f; // 予備動作中の色の強さ

  float chargeTime = 0.3f;
  float spinTime = 1.0f;
  float rotationCount = 2.0f;       // 何回転するか
  float moveSpeedMultiplier = 2.0f; // 回転中の移動速度倍率
  float cooldownTime = 0.5f;
};

// しゃがんで攻撃
struct JumpAttackParams {
  // 予備動作フェーズ（深くしゃがむ）
  float anticipationTime = 0.7f;            // 予備動作時間
  float anticipationCrouchDepth = 0.8f;     // 予備動作の沈み込み
  float anticipationColorPulseSpeed = 6.0f; // 色の点滅速度

  float chargeTime = 0.5f;
  float jumpTime = 0.7f;    // 空中にいる時間
  float jumpHeight = 4.0f;  // ジャンプの高さ
  float crouchDepth = 0.3f; // 溜め時の沈み込み
  float cooldownTime = 0.6f;
};

struct ComboAttackParams {
  // 予備動作フェーズ（素早く後退）
  float anticipationTime = 0.4f;             // 予備動作時間
  float anticipationBackstepDistance = 1.0f; // 後退距離
  float anticipationColorIntensity = 0.8f;   // 予備動作中の色の強さ

  float phaseDuration = 0.8f; // 1回ごとのコンボ時間
  float subChargeTime = 0.4f; // コンボ内での溜め
  float subRushTime = 0.2f;   // コンボ内での突進
  float rushSpeedMultiplier = 8.0f;
  float cooldownTime = 0.8f;
};

// 反撃（カウンター）
// プレイヤーから一定回数連続で被弾した後、Recovery → CounterAttack の流れで発動
struct CounterAttackParams {
  // ── トリガー条件 ──
  bool enabled =
      true; // false でカウンター挙動完全無効化（被弾しっぱなしになる点に注意）
  int triggerHitCount = 4; // 連続被弾がこの数に達した瞬間に Recovery へ
  float hitCountResetTime =
      2.5f; // この秒数攻撃を食らわなければ被弾カウントを 0 へリセット

  // ── Recovery（気合溜め・無敵）フェーズ ──
  float recoveryDuration = 1.5f; // Recovery の長さ。終了で CounterAttack へ

  // ── CounterAttack 内部フェーズ ──
  float startupTime = 0.2f;      // 起動（この間も無敵 + プレイヤー方向追尾）
  float anticipationTime = 0.5f; // 後退
  float anticipationDistance = 5.0f; // 後退距離（10.8 から控えめに調整推奨）
  float chargeTime = 0.25f;          // 溜め（ここで無敵解除）
  float rushTime = 0.55f;            // 突進
  float rushSpeedMultiplier = 15.0f; // 突進速度倍率
  float rushHomingStrength =
      1.5f; // 突進中のホーミング強度（旧 4.0 → 1.5 で読み避け可能に）
  float cooldownTime = 0.8f; // クールダウン
};

///*************************
/// 間合い取り（非攻撃行動）*************************///
// 攻撃と攻撃の「間」を作るためのパラメータ。
// 戦っている感じは攻撃の種類ではなく、この間合いの取り直しで決まる。
struct SpacingParams {
  // ── 共通 ──
  float preferredDistance = 6.0f; // 維持したい間合い
  float tooCloseDistance = 3.5f;  // これより近いと後退を選びやすくなる
  float faceRotationSpeed = 6.0f; // 間合い取り中にプレイヤーへ向く速度（rad/s）

  // ── Backstep（後退して間合いをリセット）──
  float backstepDuration = 0.45f;
  float backstepSpeedMultiplier = 2.2f;

  // ── Strafe（距離を保ったまま横移動・回り込み）──
  float strafeMinDuration = 0.8f;
  float strafeMaxDuration = 1.8f;
  float strafeSpeedMultiplier = 0.9f;
  float strafeDistanceKeepStrength = 1.5f; // 間合いのズレを詰め戻す強さ

  // ── Observe（構えて様子を見る。これが「間」を作る）──
  float observeMinDuration = 0.4f;
  float observeMaxDuration = 1.2f;

  // ── 攻撃後にどれを選ぶかの重み ──
  float backstepWeight = 1.0f;
  float strafeWeight = 1.4f;
  float observeWeight = 1.0f;
};

// PerceptionParams は知覚そのものと対で扱うため AI/EnemyAIContext.h
// に置いている。

///************************* 被弾リアクション *************************///
// 攻撃を受けた時の硬直・のけぞり・ダウンのパラメータ。
// 硬直が長いと一方的に殴れてしまい、短いと反撃が理不尽になる。
// 手触りに直結するので全部エディタから触れるようにしている。
struct DamageReactionParams {
  // ── 被弾硬直（Damage状態）──
  float staggerDuration = 1.0f; // 被弾してから動き出すまでの秒数
  // ノックバック中は硬直タイマーを進めない。
  // true だと実際の硬直は「ノックバック時間 + 上の秒数」になる。
  bool waitForKnockback = true;

  // ── 被弾時の見た目 ──
  float punchScale = 0.15f;          // ヒット時に一瞬膨らむ量
  float punchDuration = 0.25f;       // その戻り時間
  float flashDuration = 0.2f;        // 白く光ってから赤くなるまで
  float colorReturnDuration = 0.15f; // 元の色へ戻す時間
  float blinkSpeed = 50.0f;          // ダメージ点滅の速さ

  // ── のけぞり（描画のみの傾き。コライダーは傾けない）──
  float hitReactionAngle = 0.20f;    // 傾き角（ラジアン）
  float hitReactionDuration = 0.22f; // 傾いて戻るまでの秒数

  // ── ダウン（盾で弾かれた時）──
  float downedStandUpTime = 3.5f; // 起き上がるまでの秒数
  float downedWobbleSpeed = 5.0f; // ふらつきの速さ
  float downedWobbleTilt = 0.3f;  // ふらつきの傾き
};

// 各状態の攻撃パラメータをまとめた構造体
struct EnemyAttackParams {
  RushAttackParams rush;
  ChargeRushAttackParams chargeRush;
  SpinAttackParams spin;
  JumpAttackParams jump;
  ComboAttackParams combo;
  CounterAttackParams counter;
};

///************************* 基本データ *************************///
struct BattleEnemyData {
  std::string enemyId;
  std::vector<std::string> enemyIds;
  std::string modelPath;

  // 実行時のHPは BaseEnemy が持つ。ここにあるのはJSON由来の初期値だけ。
  int level = 1;
  int hp = 100;
  int attack = 15;
  int defense = 10;
  float moveSpeed = 5.0f;

  // 接近状態に入る距離
  float approachStateRange = 15.0f;
  // 攻撃状態に入る距離
  float attackStateRange = 10.0f;

  std::vector<AttackPatternType> attackPatterns = {
      AttackPatternType::Rush, AttackPatternType::Spin,
      AttackPatternType::ChargeRush, AttackPatternType::Combo,
      AttackPatternType::Jump};
  // 攻撃調整用パラメータ
  EnemyAttackParams attackParams;

  // 間合い取り（攻撃と攻撃の間の非攻撃行動）
  SpacingParams spacing;

  // 知覚（プレイヤーの状態を見て行動を変える）
  PerceptionParams perception;

  // 被弾リアクション（硬直・のけぞり・ダウン）
  DamageReactionParams damageReaction;
};

// KnockbackData は敵共通なので BaseEnemy.h へ移動した。