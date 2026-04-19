#pragma once
#include "Vector3.h"
// 軸に沿った境界ボックス（Axis-Aligned Bounding Box、AABB）の定義
struct AABB {
	Vector3 min;
	Vector3 max;
};
