#pragma once

#include "AttackModifier.h"
#include "AttackTracks.h"

#include <string>
#include <vector>

// ============================================================
// 攻撃1つ分のデータ
//
// 「突進」「跳躍」といった種別は持たない。
// どのチャンネルにどんなカーブを引いたかで動きが決まり、
// 相手依存の要素だけがモディファイアとして乗る。
//
// これにより新しい攻撃はエディタで作るだけで増やせる。
// ============================================================
struct EnemyAttack {
  std::string id;          // 選択やJSONで使う識別子
  std::string displayName; // エディタ表示名

  // 攻撃全体の長さ（秒）。カーブの進行度 0〜1 はこの時間に対応する。
  float duration = 1.5f;

  // ── 動き ──
  // 位置はカーブか経路のどちらかから取る。回転とスケールは常にカーブ。
  AttackPositionSource positionSource = AttackPositionSource::Curves;
  std::string pathName; // positionSource == Path のとき参照する経路名

  AttackTracks tracks;

  // ── 相手依存の要素 ──
  std::vector<AttackModifier> modifiers;

  // ── いつ選ばれるか ──
  float minRange = 0.0f;      // これより近いと選ばれない
  float maxRange = 999.0f;    // これより遠いと選ばれない
  float weight = 1.0f;        // 抽選の重み。0で抽選対象外（専用経路でのみ発動）
  float cooldown = 0.0f;      // この攻撃自体の再使用待ち時間（秒）
  float selfHpBelow = 1.0f;   // 自分のHP割合がこれ以下でのみ使う
  float targetHpBelow = 1.0f; // 相手のHP割合がこれ以下でのみ使う

  // ── 性質 ──
  bool parriable = false; // 盾で受けるとダウンするか
  bool fast = true;       // 発生が速く短い隙に差し込めるか（重み補正に使う）

  // 攻撃判定を持つモディファイアがあるか（エディタの警告用）
  bool HasHitbox() const;
};

// ============================================================
// JSON 入出力
// ============================================================
namespace EnemyAttackIO {

inline const char *kDefaultPath =
    "Resources/Json/BattleEnemies/enemy_attacks.json";

bool Load(const std::string &path, std::vector<EnemyAttack> &out);
bool Save(const std::string &path, const std::vector<EnemyAttack> &attacks);

} // namespace EnemyAttackIO
