#pragma once

#include "MathFunc.h"
#include "Vector3.h"

#include <string>

// ============================================================
// 経路をどの座標系で解釈するか
//
// 同じ制御点でも基準を変えるだけで性格が変わる。
//   SelfLocal      … 攻撃開始時の自分が基準。踏み込みや後退はこれ
//   TargetRelative …
//   相手の現在位置が基準。相手が動けば経路も追従する（＝ホーミング） World …
//   絶対座標。決まった場所へ落とす技など
// ============================================================
enum class MotionSpace {
  SelfLocal,
  TargetRelative,
  World,
};

const char *MotionSpaceToString(MotionSpace space);
MotionSpace MotionSpaceFromString(const std::string &name);

// ============================================================
// 経路の評価に必要な周辺情報
//
// 経路そのものは「開始時の自分」や「相手」を知らないので、
// 評価のたびにこの構造体で外から渡す。
// ============================================================
struct MotionContext {
  // 攻撃（または弾）が始まった瞬間の位置と向き
  Vector3 startPosition{};
  float startYaw = 0.0f;

  // 相手の現在位置。毎フレーム更新されるので
  // TargetRelative の経路は自動的に追従する。
  Vector3 targetPosition{};
  bool hasTarget = false;

  // 経路全体の拡大率。同じ形のまま射程だけ変えたいときに使う。
  float scale = 1.0f;

  /// <summary>
  /// ローカル座標（前方＝+Z）をワールドへ変換する
  /// </summary>
  Vector3 LocalToWorld(const Vector3 &local, MotionSpace space) const;

  /// <summary>
  /// LocalToWorld の逆変換。
  /// ギズモで動かしたワールド位置を制御点へ書き戻すのに使う。
  /// </summary>
  Vector3 WorldToLocal(const Vector3 &world, MotionSpace space) const;
};
