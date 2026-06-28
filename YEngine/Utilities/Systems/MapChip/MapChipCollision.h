#pragma once

#include "MapChipField.h"
#include "Vector3.h"
#include <functional>
#include <vector>
#include <stdint.h>

///************************* 衝突方向列挙 *************************///
/// 衝突方向を定義する
enum CollisionDirection {
	NoneDir = 0,
	LeftDir = 1,
	RightDir = 2,
	TopDir = 3,
	BottomDir = 4
};

///************************* 衝突情報構造体 *************************///
/// 衝突結果の情報を保持する
struct CollisionInfo {
	uint32_t xIndex = 0;
	uint32_t yIndex = 0;
	MapChipType blockType = MapChipType::kBlank;
	CollisionDirection direction;
	float penetrationDepth = 0.0f;
	MapChipField::Rect blockRect;
};

///************************* マップチップ衝突判定クラス *************************///
/// マップ上のブロックとの衝突検出と解決を行う
class MapChipCollision {
public:
	///************************* コライダ構造体 *************************///
	/// 移動オブジェクト用の当たり判定矩形を表す
	struct ColliderRect {
		float width;
		float height;
		float offsetX;
		float offsetY;

		///************************* コンストラクタ *************************///
		/// 初期サイズとオフセットを設定する
		ColliderRect(float w = 1.0f, float h = 1.0f, float ox = 0.0f, float oy = 0.0f)
			: width(w), height(h), offsetX(ox), offsetY(oy) {
		}
	};

	///************************* 衝突フラグ *************************///
	/// 衝突方向を表すビットフラグ
	enum CollisionFlag {
		None = 0,
		Left = 1 << 0,
		Right = 1 << 1,
		Top = 1 << 2,
		Bottom = 1 << 3,
		All = Left | Right | Top | Bottom
	};

	///************************* コンストラクタ *************************///
	/// マップデータを受け取り初期化する
	MapChipCollision(MapChipField* mapChipField) : mapChipField_(mapChipField) {}

	///************************* 衝突検出・解決 *************************///
	/// 位置と速度をもとに衝突を検出し補正を行う
	void DetectAndResolveCollision(
		const ColliderRect& colliderRect,
		Vector3& position,
		Vector3& velocity,
		int checkFlags = CollisionFlag::All,
		std::function<void(const CollisionInfo&)> collisionCallback = nullptr);

private:
	///************************* メンバ変数 *************************///
	/// マップチップデータへの参照
	MapChipField* mapChipField_;
};
