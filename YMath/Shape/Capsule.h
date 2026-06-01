#pragma once
#include "Vector3.h"

// カプセル形状の定義
// 線分 (start → end) を中心軸として、その周囲を radius で覆う形状。
// 両端は半球で閉じられる。
struct Capsule {
    Vector3 start;  // 線分の始点
    Vector3 end;    // 線分の終点
    float   radius; // 半径
};
