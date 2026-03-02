#pragma once
// Math
#include "Vector3.h"
#include "Quaternion.h"
#include "Matrix4x4.h"
#include "MathFunc.h"


enum class LoopType {
	Once,			  // 一度だけ再生
	Loop,			  // ループ
	Reverse,		  // 逆再生
	PingPong,		  // 往復して再生
	Clamp,			  // 最後までいったら静止
};