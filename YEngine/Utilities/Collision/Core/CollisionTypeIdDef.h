#pragma once
// C++
#include <cstdint>
// コリジョン種別IDを定義
enum class CollisionTypeIdDef : uint32_t
{
	kNone = 0,					// 当たり判定なし
	kPlayer,					// プレイヤー
	kEnemy,						// 敵
	kFieldEnemy,				// フィールド敵
	kBattleEnemy,				// バトル敵
	kPlayerWeapon,				// プレイヤーの武器
	kPlayerShield,				// プレイヤーの盾
};