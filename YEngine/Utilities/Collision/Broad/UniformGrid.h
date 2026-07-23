#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "Vector3.h"
#include "Vector4.h"
#include "Shape/AABB.h"

class BaseCollider;
class InstancedCube;

namespace YoRigine {

	// ============================================================
	// Uniform Grid (3D ハッシュグリッド) Broad Phase
	//
	// - 世界をセルサイズ単位で区切り、コライダーをAABBが重なる全セルに登録する
	// - ペアクエリは、同じセルに居るコライダー同士だけを候補として列挙する
	// - 同じペアが複数セルで報告されるのを防ぐため、内部で「報告済みペア集合」を持つ
	// - セルサイズは Configure() で変更可能。デフォルト 5.0
	// ============================================================
	class UniformGrid {
	public:
		struct CellKey {
			int32_t x;
			int32_t y;
			int32_t z;
			bool operator==(const CellKey& o) const noexcept {
				return x == o.x && y == o.y && z == o.z;
			}
		};
		struct CellKeyHash {
			size_t operator()(const CellKey& k) const noexcept {
				// 三軸の int をミックスする簡易ハッシュ
				size_t h = static_cast<size_t>(k.x) * 73856093u;
				h ^= static_cast<size_t>(k.y) * 19349663u;
				h ^= static_cast<size_t>(k.z) * 83492791u;
				return h;
			}
		};

		void SetCellSize(float size);
		float GetCellSize() const { return cellSize_; }

		// 1フレーム分のデータを掃除する
		void Clear();

		// コライダーを登録 (AABB が重なる全セルに入る)
		void Insert(BaseCollider* c, const AABB& aabb);

		// 同セルに居るコライダー同士のペア候補を列挙する。
		// 重複ペアは内部で除去する。
		void QueryPairs(std::vector<std::pair<BaseCollider*, BaseCollider*>>& outPairs);

		// 指定AABBと重なるセルに登録されたコライダーを重複なしで取得する。
		// layerMask は候補生成時の早期除外に使う（精密形状判定は呼び出し側で行う）。
		void QueryAABB(const AABB& area, uint32_t layerMask,
			std::vector<BaseCollider*>& outColliders) const;

		// ============================================================
		// デバッグ描画: カメラ位置周辺 (radius 範囲) のセルを全て描く。
		// 空セルも含めて全て出すので、ノイズを下げたいなら radius を絞る。
		// 色は AABB ワイヤーでそのまま使う。
		// ============================================================
		void DrawDebugAroundCamera(InstancedCube* cubes,
			const Vector3& cameraPos, float radius, const Vector4& color) const;

	private:
		void CellRangeFor(const AABB& aabb, CellKey& outMin, CellKey& outMax) const;

		struct PairHash {
			size_t operator()(const std::pair<BaseCollider*, BaseCollider*>& p) const noexcept {
				auto h1 = std::hash<BaseCollider*>{}(p.first);
				auto h2 = std::hash<BaseCollider*>{}(p.second);
				return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
			}
		};

	private:
		float cellSize_ = 2.5f;
		float invCellSize_ = 1.0f / 2.5f;

		std::unordered_map<CellKey, std::vector<BaseCollider*>, CellKeyHash> cells_;
		std::unordered_set<std::pair<BaseCollider*, BaseCollider*>, PairHash> seenPairsScratch_;
		std::vector<BaseCollider*> overflowColliders_;
		mutable std::unordered_set<BaseCollider*> querySeenScratch_;
	};

} // namespace YoRigine
