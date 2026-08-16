#pragma once

#include "Vector3.h"

#include <string>

// ============================================================
// モディファイア（カーブでは表現できない要素）
//
// 位置・回転・スケールは形が決まっているのでカーブで書けるが、
// 「相手を向く」「追尾する」のように実行時の状況で結果が変わるものは
// カーブに焼き込めない。そういうものだけをここに置く。
//
// 振り分けの基準は「相手の状態に依存するかどうか」だけ。
// 依存しないならカーブ、依存するならモディファイア。
// この基準がある限り種類は増え続けない。
// ============================================================
enum class AttackModifierType {
  FaceTarget,     // 相手の方向を向き続ける
  HomingOffset,   // 位置を相手の方向へ寄せる（カーブで作った軌道を曲げる）
  Hitbox,         // 接触ダメージを発生させる区間
  Invincible,     // 無敵になる区間
  EmitProjectile, // 弾を撃つ
};

const char *AttackModifierTypeToString(AttackModifierType type);
AttackModifierType AttackModifierTypeFromString(const std::string &name);

// 種別ごとの説明（エディタのツールチップ用）
const char *GetAttackModifierDescription(AttackModifierType type);

// ============================================================
// モディファイア1つ分
//
// 種別ごとに使うフィールドが違う。共用体にすると JSON もエディタも
// 面倒になるだけなので、素直に全部持たせて未使用は無視する。
// ============================================================
struct AttackModifier {
  AttackModifierType type = AttackModifierType::Hitbox;

  // 効果のある時間範囲（攻撃開始からの秒数）。
  // EmitProjectile のような瞬間的なものは startTime だけ見る。
  float startTime = 0.0f;
  float endTime = 0.1f;

  // ── FaceTarget / HomingOffset ──
  // FaceTarget では回転速度(rad/s)、HomingOffset では寄せの強さ
  float strength = 6.0f;

  // ── Hitbox ──
  // 同じ番号の間は1回しか命中しない。多段攻撃は段ごとに変える。
  int damageWindow = 0;

  // ── EmitProjectile ──
  std::string projectileId;         // 撃つ弾の定義ID
  int count = 1;                    // 一度に撃つ数
  float spreadDeg = 0.0f;           // 扇状に散らす角度（count で等分）
  Vector3 offset{0.0f, 1.0f, 0.0f}; // 発射位置のローカルオフセット
  bool aimAtTarget = true;          // 相手へ向けて撃つ（false で正面へ）

  // 指定時刻がこのモディファイアの範囲内か
  bool IsActiveAt(float time) const {
    return time >= startTime && time <= endTime;
  }

  // 瞬間的に1回だけ発火する種別か（弾の発射など）
  bool IsInstant() const { return type == AttackModifierType::EmitProjectile; }
};
