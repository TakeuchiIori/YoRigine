#pragma once

#include "Vector3.h"
#include "Vector4.h"

#include <string>
#include <vector>

// ============================================================
// 攻撃フェーズの種別
//
// 「攻撃1つ = C++のStateクラス1つ」だと、攻撃を増やすたびに
// クラス・enum・重み計算のswitch・エディタを全部触ることになる。
// 攻撃をこのフェーズの並びとして表現すれば、新しい攻撃はJSONに
// 1エントリ足すだけで作れる。
//
// ここに並ぶのは「C++でしか書けない動きの原型」だけで、
// 攻撃そのものはこれらの組み合わせとしてデータ側に置く。
// ============================================================
enum class AttackPhaseType {
  Anticipation, // 予備動作。指定方向へずれて溜めを作る（後退・沈み込み・ひねり）
  Charge,       // その場で溜める
  Dash,         // 固定方向へ直進する
  Spin,         // 回転しながら相手へ寄る
  Leap,         // 放物線を描いて着地点へ跳ぶ
  Wait,         // 硬直（何もしない）
  Scripted,     // JSONで書けない特殊挙動をC++側から差し込むための逃げ道
};

const char *AttackPhaseTypeToString(AttackPhaseType type);
AttackPhaseType AttackPhaseTypeFromString(const std::string &name);

// ============================================================
// 予備動作のずれ方（カーブ）
//
// 既存の攻撃がそれぞれ違うカーブで動いていたので、
// 挙動を変えずに移行できるよう種類を持たせている。
// ============================================================
enum class OffsetCurve {
  EaseOutCubic, // 一気にずれて減速（突進の後退）
  EaseOutQuad,  // 上より緩やか（コンボの後退）
  Hump,         // ずれて戻る山（踏み込み）
  SineIn,       // ずれたまま留まる（しゃがみ）
  Linear,
};

const char *OffsetCurveToString(OffsetCurve curve);
OffsetCurve OffsetCurveFromString(const std::string &name);

// ============================================================
// 攻撃フェーズ1つ分
// ============================================================
struct AttackPhase {
  AttackPhaseType type = AttackPhaseType::Wait;
  std::string label; // エディタ表示用のメモ
  float duration = 0.5f;

  // ── 移動 ──
  // offsetAxis は敵から見たローカル方向。{0,0,-1}=後退 / {0,-1,0}=沈み込み
  float distance = 0.0f;
  Vector3 offsetAxis{0.0f, 0.0f, -1.0f};
  OffsetCurve offsetCurve = OffsetCurve::EaseOutCubic;

  float speedMultiplier =
      0.0f;               // Dash / Spin の移動速度倍率（moveSpeed に掛ける）
  float homing = 0.0f;    // Dash 中に進行方向を相手へ寄せる強さ（0=直進）
  float height = 0.0f;    // Leap の放物線の高さ
  float rotations = 0.0f; // Spin の回転数 / Anticipation のひねり角(rad)

  // ── 向き ──
  bool faceTarget = false; // 毎フレーム相手を向き続ける

  // ── 状態 ──
  bool invincible = false; // このフェーズ中は無敵

  // ── 攻撃判定 ──
  // -1 で判定なし。同じ番号が続く間は1回しか命中しないので、
  // コンボの各段には別々の番号を振る。
  int damageWindow = -1;

  // ── 見た目 ──
  // スケールは基準スケールに対する倍率。開始時に from→to のアニメを流す。
  Vector3 scaleFrom{1.0f, 1.0f, 1.0f};
  Vector3 scaleTo{1.0f, 1.0f, 1.0f};
  float scaleTime = 0.0f; // 0ならフェーズ長と同じ

  bool useColor = false;
  Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};

  bool shake = false; // チャージ中の震え
  float shakePower = 0.2f;

  std::string vfxName;    // フェーズ開始時に出す Composite 名
  std::string scriptedId; // type == Scripted のときに呼ぶ処理の名前
};

// ============================================================
// 攻撃1つ分のデータ
// ============================================================
struct EnemyAttackAction {
  std::string id;          // "rush" など。JSONと選択の識別子
  std::string displayName; // エディタ表示名

  std::vector<AttackPhase> phases;

  // ── 繰り返し（コンボ用）──
  // [loopBegin, loopEnd) のフェーズを loopCount 回繰り返す。
  // loopBegin < 0 で繰り返しなし。
  int loopBegin = -1;
  int loopEnd = -1;
  int loopCount = 1;
  float loopSpeedGain = 0.0f; // 周回ごとに Dash の速度倍率へ加算

  // ── いつ選ばれるか ──
  float minRange = 0.0f;      // これより近いと選ばれない
  float maxRange = 999.0f;    // これより遠いと選ばれない
  float weight = 1.0f;        // 抽選の基本重み
  float cooldown = 0.0f;      // この攻撃自体の再使用待ち時間（秒）
  float selfHpBelow = 1.0f;   // 自分のHP割合がこれ以下でのみ使う
  float targetHpBelow = 1.0f; // 相手のHP割合がこれ以下でのみ使う
  int phaseGate = -1;         // ボスのフェーズ番号。-1で常時

  // ── 性質 ──
  bool parriable = false; // 盾で受けるとダウンするか
  bool fast = true;       // 発生が速く短い隙に差し込めるか（重み補正に使う）

  // 全フェーズの合計時間（繰り返しを含む）
  float TotalDuration() const;
};

// ============================================================
// JSON 入出力
// ============================================================
namespace EnemyAttackActionIO {

inline const char *kDefaultPath =
    "Resources/Json/BattleEnemies/attack_actions.json";

bool Load(const std::string &path, std::vector<EnemyAttackAction> &out);
bool Save(const std::string &path,
          const std::vector<EnemyAttackAction> &actions);

} // namespace EnemyAttackActionIO
