#pragma once
// C++
#include <cstdint>
#include <string>
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

	// ── エディター配置オブジェクト用 ──────────────────────────
	kStaticWall,				// 壁・建物: プレイヤーと敵の移動を遮る
	kNavObstacle,				// NavMesh障害物: 経路探索で通行不可として扱う
	kNavTrigger,				// エリアトリガー: 部屋の入口など進入検知に使う
	kWaypoint,					// 巡回ウェイポイント: 敵のPatrolルートの目標点
};

// ── CollisionTypeIdDef ユーティリティ ──────────────────────────────────────
// エディターのドロップダウン表示名とIDの相互変換

inline const char* CollisionTypeIdToString(CollisionTypeIdDef id)
{
	switch (id) {
	case CollisionTypeIdDef::kNone:         return "None";
	case CollisionTypeIdDef::kPlayer:       return "Player";
	case CollisionTypeIdDef::kEnemy:        return "Enemy";
	case CollisionTypeIdDef::kFieldEnemy:   return "FieldEnemy";
	case CollisionTypeIdDef::kBattleEnemy:  return "BattleEnemy";
	case CollisionTypeIdDef::kPlayerWeapon: return "PlayerWeapon";
	case CollisionTypeIdDef::kPlayerShield: return "PlayerShield";
	case CollisionTypeIdDef::kStaticWall:   return "StaticWall";
	case CollisionTypeIdDef::kNavObstacle:  return "NavObstacle";
	case CollisionTypeIdDef::kNavTrigger:   return "NavTrigger";
	case CollisionTypeIdDef::kWaypoint:     return "Waypoint";
	default:                                return "Unknown";
	}
}

// エディターのドロップダウン用：配置オブジェクトに設定できる種別のみ
inline constexpr CollisionTypeIdDef kPlacedObjectColliderTypes[] = {
	CollisionTypeIdDef::kNone,
	CollisionTypeIdDef::kStaticWall,
	CollisionTypeIdDef::kNavObstacle,
	CollisionTypeIdDef::kNavTrigger,
	CollisionTypeIdDef::kWaypoint,
};