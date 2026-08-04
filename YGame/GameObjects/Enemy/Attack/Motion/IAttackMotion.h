#pragma once

#include "AttackMotionTypes.h"

#include <memory>
#include <string>

// ============================================================
// 攻撃中の動きを表すインターフェース
//
// 「突進」「跳躍」「後退」といった形ごとに C++ の関数を用意すると、
// 新しい動きを作るたびにコードを触ることになる。
// ここでは動きを「進行度 t を position に変換するもの」として抽象化し、
// 実際の形は制御点などのデータ側で決める。
//
// 敵本体の動きと弾の動きで同じ実装を使い回す。
// ============================================================
class IAttackMotion {
public:
  virtual ~IAttackMotion() = default;

  /// <summary>
  /// 進行度から位置を求める
  /// </summary>
  /// <param name="t">0（開始）〜1（終了）</param>
  /// <param name="ctx">開始位置や相手位置などの周辺情報</param>
  /// <returns>ワールド座標</returns>
  virtual Vector3 Evaluate(float t, const MotionContext &ctx) const = 0;

  /// <summary>
  /// 進行方向。既定では少し先の位置との差分から求める。
  /// 弾の向きや敵の向きを経路に沿わせるのに使う。
  /// </summary>
  virtual Vector3 EvaluateDirection(float t, const MotionContext &ctx) const;

  virtual const char *GetTypeName() const = 0;
  virtual std::unique_ptr<IAttackMotion> Clone() const = 0;
};
