#pragma once

#include "IAttackMotion.h"

// ============================================================
// 相手（または開始地点）の周りを回る経路
//
// 円周上に制御点を並べればスプラインでも書けるが、
// 「半径5mで180度回り込む」を点で置くのは手間なので
// パラメータで指定できる形も用意しておく。
//
// 弾に使えば渦を巻きながら飛ぶ弾になる。
// ============================================================
class OrbitMotion : public IAttackMotion {
public:
  // 回る中心をどこに置くか。TargetRelative なら相手を中心に回る。
  MotionSpace space = MotionSpace::TargetRelative;

  float startRadius = 6.0f; // 開始時の半径
  float endRadius = 2.0f;   // 終了時の半径（詰めながら回るなら小さく）

  float startAngleDeg = 0.0f; // 開始角度（中心から見た方向。0で+Z側）
  float sweepDeg = 180.0f;    // 回る角度。負で逆回り

  float startHeight = 0.0f; // 開始時の高さ
  float endHeight = 0.0f;   // 終了時の高さ

  Vector3 Evaluate(float t, const MotionContext &ctx) const override;
  const char *GetTypeName() const override { return "orbit"; }
  std::unique_ptr<IAttackMotion> Clone() const override;
};
