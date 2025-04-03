#pragma once
// C++
#include <cstdint>
// コリジョン種別IDを定義
enum class CollisionTypeIdDef : uint32_t
{
	kDefault = 0,	// デフォルト
	kPlayer,		// プレイヤー
	kNextFramePlayer,
	kPlayerBody,	// プレイヤー体
	kGrass,			// 草
	kEnemy,			// 敵
	kNone			// 当たり判定なし
};