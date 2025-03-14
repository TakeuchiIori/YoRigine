#include "MapChipCollision.h"

void MapChipCollision::DetectAndResolveCollision(
	const ColliderRect& colliderRect,
	Vector3& position,
	Vector3& velocity,
	int checkFlags,
	std::function<void(const CollisionInfo&)> collisionCallback) {
	// 衝突情報のリスト
	std::vector<CollisionInfo> collisions;

	// 現在位置からマップチップインデックスを取得
	MapChipField::IndexSet currentIndex = mapChipField_.GetMapChipIndexSetByPosition(position);

	// 検査範囲（最適化のため、必要最小限のブロックだけチェック）
	int searchRadius = 2; // 移動速度が速い場合は大きくする

	// 検査範囲内のブロックをチェック
	for (int dy = -searchRadius; dy <= searchRadius; dy++) {
		for (int dx = -searchRadius; dx <= searchRadius; dx++) {
			// チェック対象のインデックスを計算
			int checkX = static_cast<int>(currentIndex.xIndex) + dx;
			int checkY = static_cast<int>(currentIndex.yIndex) + dy;

			// マップ範囲外ならスキップ
			if (checkX < 0 || checkY < 0 ||
				checkX >= static_cast<int>(MapChipField::GetNumBlockHorizontal()) ||
				checkY >= static_cast<int>(MapChipField::GetNumBlockVertical())) {
				continue;
			}

			uint32_t xIndex = static_cast<uint32_t>(checkX);
			uint32_t yIndex = static_cast<uint32_t>(checkY);

			// ブロックタイプを取得
			MapChipType blockType = mapChipField_.GetMapChipTypeByIndex(xIndex, yIndex);

			// 空白ブロックならスキップ
			if (blockType == MapChipType::kBlank) {
				continue;
			}

			// ブロックの矩形を取得
			MapChipField::Rect blockRect = mapChipField_.GetRectByIndex(xIndex, yIndex);

			// オブジェクトの現在の矩形を計算
			MapChipField::Rect objectRect = {
				position.x - colliderRect.width / 2.0f + colliderRect.offsetX,
				position.x + colliderRect.width / 2.0f + colliderRect.offsetX,
				position.y - colliderRect.height / 2.0f + colliderRect.offsetY,
				position.y + colliderRect.height / 2.0f + colliderRect.offsetY
			};

			// 移動後の予測位置
			MapChipField::Rect nextObjectRect = {
				objectRect.left + velocity.x,
				objectRect.right + velocity.x,
				objectRect.bottom + velocity.y,
				objectRect.top + velocity.y
			};

			// 矩形同士の衝突をチェック
			bool willCollideX = nextObjectRect.right > blockRect.left &&
				nextObjectRect.left < blockRect.right;
			bool willCollideY = nextObjectRect.top > blockRect.bottom &&
				nextObjectRect.bottom < blockRect.top;

			if (willCollideX && willCollideY) {
				// 衝突情報を作成
				CollisionInfo info;
				info.xIndex = xIndex;
				info.yIndex = yIndex;
				info.blockType = blockType;
				info.blockRect = blockRect;

				// 衝突方向と深さを計算
				// 水平方向
				float leftPenetration = nextObjectRect.right - blockRect.left;
				float rightPenetration = blockRect.right - nextObjectRect.left;

				// 垂直方向
				float topPenetration = nextObjectRect.top - blockRect.bottom;
				float bottomPenetration = blockRect.top - nextObjectRect.bottom;

				// 最小めり込み方向を探す
				float minPenetration = std::numeric_limits<float>::max();
				CollisionDirection  collisionDirection{};

				/// 左方向の衝突チェック
				if ((checkFlags & CollisionFlag::Left) && velocity.x > 0 && leftPenetration < minPenetration) {
					minPenetration = leftPenetration;
					collisionDirection = CollisionDirection::LeftDir;
				}

				/// 右方向の衝突チェック
				if ((checkFlags & CollisionFlag::Right) && velocity.x < 0 && rightPenetration < minPenetration) {
					minPenetration = rightPenetration;
					collisionDirection = CollisionDirection::RightDir;
				}

				/// 上方向の衝突チェック
				if ((checkFlags & CollisionFlag::Top) && velocity.y > 0 && topPenetration < minPenetration) {
					minPenetration = topPenetration;
					collisionDirection = CollisionDirection::TopDir;
				}

				/// 下方向の衝突チェック
				if ((checkFlags & CollisionFlag::Bottom) && velocity.y < 0 && bottomPenetration < minPenetration) {
					minPenetration = bottomPenetration;
					collisionDirection = CollisionDirection::BottomDir;
				}

				// 衝突情報を設定
				info.direction = collisionDirection;
				info.penetrationDepth = minPenetration;

				// 衝突リストに追加
				collisions.push_back(info);
			}
		}
	}

	// 衝突が検出されなければ終了
	if (collisions.empty()) {
		return;
	}

	// 衝突解決（最も浅い衝突から順に処理）
	std::sort(collisions.begin(), collisions.end(),
		[](const CollisionInfo& a, const CollisionInfo& b) {
			return a.penetrationDepth < b.penetrationDepth;
		});

	// 各衝突を処理
	for (const auto& collision : collisions) {
		// 衝突方向に応じた処理
		switch (collision.direction) {
		case LeftDir:
			position.x = collision.blockRect.left - colliderRect.width / 2.0f - colliderRect.offsetX;
			velocity.x = 0;
			break;

		case RightDir:
			position.x = collision.blockRect.right + colliderRect.width / 2.0f - colliderRect.offsetX;
			velocity.x = 0;
			break;

		case TopDir:
			position.y = collision.blockRect.bottom - colliderRect.height / 2.0f - colliderRect.offsetY;
			velocity.y = 0;
			break;

		case BottomDir:
			position.y = collision.blockRect.top + colliderRect.height / 2.0f - colliderRect.offsetY;
			velocity.y = 0;
			break;
		}

		// コールバック関数が設定されていれば呼び出す
		if (collisionCallback) {
			collisionCallback(collision);
		}
	}
}