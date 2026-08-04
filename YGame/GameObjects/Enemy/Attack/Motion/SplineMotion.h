#pragma once

#include "IAttackMotion.h"

#include <vector>

// ============================================================
// 制御点を通る経路
//
// これ1つで従来の突進・後退・跳躍・弧を描く斬りつけが全部書ける。
//   突進   … (0,0,0) → (0,0,8)
//   後退   … (0,0,0) → (0,0,-1.5)
//   跳躍   … (0,0,0) → (0,3,4) → (0,0,8)
//   回り込み … (0,0,0) → (4,0,3) → (2,0,7) → (0,0,8)
//
// 制御点を足すだけで新しい動きが作れるので、
// 動きの形ごとに C++ の関数を書く必要がない。
// ============================================================
class SplineMotion : public IAttackMotion {
public:
  // 制御点。space で解釈される座標系のローカル値。
  std::vector<Vector3> points;

  // 制御点をどの基準で読むか
  MotionSpace space = MotionSpace::SelfLocal;

  // 制御点間を等速で進むか。
  // false だと制御点の密度で速度が変わり、密なところで減速する。
  // true にすると弧長で正規化して見た目の速度が一定になる。
  bool constantSpeed = true;

  Vector3 Evaluate(float t, const MotionContext &ctx) const override;
  const char *GetTypeName() const override { return "spline"; }
  std::unique_ptr<IAttackMotion> Clone() const override;

  // 制御点が2つ未満だと補間できないので、その判定
  bool IsValid() const { return points.size() >= 2; }

private:
  // 制御点を Catmull-Rom で補間する（ローカル座標のまま）
  Vector3 SampleLocal(float t) const;

  // 弧長テーブルを作り、進行度を「距離の割合」へ変換する
  void EnsureArcTable() const;
  float RemapByArcLength(float t) const;

private:
  // 弧長テーブル。points が変わったら作り直す。
  mutable std::vector<float> arcTable_;
  mutable size_t arcTableSourceCount_ = 0;
};
