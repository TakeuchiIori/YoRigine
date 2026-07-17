#pragma once

#include "Vector3.h"

class Player;

// ============================================================
// 魔法の発射起点・前方向の共通算出
// 当たり判定(MagicAttackInstance)とVFX(MagicVfxEventHandler)で
// まったく同じ座標を使うため、計算をここ1か所へ集約する。
// 片方だけ変えて「判定と見た目がズレる」事故を防ぐのが目的。
// ============================================================
namespace MagicCastGeometry {
// カメラ向きから水平前方向（正規化済み）を得る。ほぼ真上/真下でも安全な既定を返す。
Vector3 ResolveForward(Player &player);
// プレイヤー胸元やや前方の詠唱起点。
Vector3 ResolveCastOrigin(Player &player);
// 指定位置の直下の地面座標を返す（着弾リング等の地面系VFX用）。
// 上方から下向きに地形(kGroundSurface/kStaticWall/kNavObstacle)へレイを撃ち、
// 当たらなければ Y=0 の平面へフォールバックする。返り値はわずかに浮かせる。
Vector3 ResolveGroundPoint(const Vector3 &position);
} // namespace MagicCastGeometry
