#pragma once

#include "../Motion/IAttackMotion.h"
#include "Vector4.h"

#include <memory>
#include <string>

// ============================================================
// 弾の飛び方
//
//   Straight … 発射方向へまっすぐ。速度と寿命で飛距離が決まる
//   Path     … 経路データに沿って飛ぶ。スプラインなら曲がる弾、
//              円運動なら渦を巻く弾になる
//
// Path のとき経路の空間を TargetRelative にすれば追尾弾になる。
// 「弾の挙動を変えたい」はここを差し替えるだけで済む。
// ============================================================
enum class ProjectileMotionMode {
  Straight,
  Path,
};

const char *ProjectileMotionModeToString(ProjectileMotionMode mode);
ProjectileMotionMode ProjectileMotionModeFromString(const std::string &name);

// ============================================================
// 弾1発分の定義
// ============================================================
struct ProjectileDefinition {
  std::string id;

  // ── 飛び方 ──
  ProjectileMotionMode mode = ProjectileMotionMode::Straight;

  // Straight のとき使う
  float speed = 12.0f;
  float gravity = 0.0f; // 正で落下する
  float homing = 0.0f;  // 進行方向を相手へ寄せる強さ（0で直進）

  // Path のとき使う。経路の進行度は寿命に対する割合。
  std::shared_ptr<IAttackMotion> path;

  // ── 寿命と当たり判定 ──
  float lifeTime = 3.0f;
  float radius = 0.5f;
  int damage = 10;
  bool destroyOnHit = true;

  // ── 見た目 ──
  std::string modelPath; // 空ならモデルなし（VFXのみ）
  Vector3 scale{0.5f, 0.5f, 0.5f};
  Vector4 color{1.0f, 0.6f, 0.2f, 1.0f};
  std::string trailVfxName; // 飛行中に追従させる Composite 名
  std::string hitVfxName;   // 着弾時に出す Composite 名

  // 進行方向へ機首を向けるか
  bool faceVelocity = true;
};

// ============================================================
// 弾の発射指示
//
// 攻撃のどの時点で、どの弾を、何発、どう散らして撃つか。
// 1つの攻撃に複数の発射指示を持たせられるので、
// 「前方3方向 → 少し遅れて全方位」のような構成も作れる。
// ============================================================
struct ProjectileEmit {
  std::string projectileId; // 撃つ弾の定義ID

  // 発射タイミング。フェーズ開始からの秒数。
  float time = 0.0f;

  int count = 1;             // 一度に撃つ数
  float spreadDeg = 0.0f;    // 扇状に散らす角度（count で等分）
  float intervalTime = 0.0f; // count 発を撃つ間隔（0なら同時）

  Vector3 offset{0.0f, 1.0f, 0.0f}; // 発射位置のローカルオフセット
  bool aimAtTarget = true;          // 相手へ向けて撃つか（false なら正面へ）
};
