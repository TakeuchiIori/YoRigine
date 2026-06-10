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
	kGroundSurface,				// 地面表面: スタンプ配置等のRaycast対象 (narrow-phase衝突不参加)
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
	case CollisionTypeIdDef::kStaticWall:    return "StaticWall";
	case CollisionTypeIdDef::kNavObstacle:   return "NavObstacle";
	case CollisionTypeIdDef::kNavTrigger:    return "NavTrigger";
	case CollisionTypeIdDef::kWaypoint:      return "Waypoint";
	case CollisionTypeIdDef::kGroundSurface: return "GroundSurface";
	default:                                 return "Unknown";
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

// ============================================================
// レイヤービット定義
//   BaseCollider の layerBits_ / collisionMask_ で使うビット位置。
//   uint32_t なので最大 32 層。
// ============================================================
enum class CollisionLayer : uint32_t {
	Default      = 0,  // 既定 (どこにも属さないものはここ)
	Player       = 1,
	Enemy        = 2,
	PlayerWeapon = 3,
	PlayerShield = 4,
	EnemyWeapon  = 5,
	StaticWall   = 6,
	NavObstacle  = 7,
	NavTrigger   = 8,
	Waypoint     = 9,
	Pickup       = 10,
	Trigger      = 11,
	// 12-31 はユーザー拡張用
};

inline constexpr uint32_t CollisionLayerBit(CollisionLayer layer) {
	return 1u << static_cast<uint32_t>(layer);
}

// すべてに反応するマスク (旧来の挙動)
inline constexpr uint32_t kCollisionMaskAll = 0xFFFFFFFFu;

// CollisionTypeIdDef → CollisionLayer の自動対応 (Initialize から呼ぶと便利)
inline CollisionLayer LayerFromTypeId(CollisionTypeIdDef id) {
	switch (id) {
	case CollisionTypeIdDef::kPlayer:       return CollisionLayer::Player;
	case CollisionTypeIdDef::kEnemy:        return CollisionLayer::Enemy;
	case CollisionTypeIdDef::kFieldEnemy:   return CollisionLayer::Enemy;
	case CollisionTypeIdDef::kBattleEnemy:  return CollisionLayer::Enemy;
	case CollisionTypeIdDef::kPlayerWeapon: return CollisionLayer::PlayerWeapon;
	case CollisionTypeIdDef::kPlayerShield: return CollisionLayer::PlayerShield;
	case CollisionTypeIdDef::kStaticWall:   return CollisionLayer::StaticWall;
	case CollisionTypeIdDef::kNavObstacle:  return CollisionLayer::NavObstacle;
	case CollisionTypeIdDef::kNavTrigger:   return CollisionLayer::NavTrigger;
	case CollisionTypeIdDef::kWaypoint:     return CollisionLayer::Waypoint;
	default:                                return CollisionLayer::Default;
	}
}