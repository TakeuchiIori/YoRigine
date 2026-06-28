#include "UniformGrid.h"
#include <cmath>
#include <algorithm>

#include <Graphics/Drawer/InstancedShape/InstancedCube.h>
#include <Debugger/Logger.h>

namespace YoRigine {

	void UniformGrid::SetCellSize(float size) {
		if (size < 0.01f) size = 0.01f;
		cellSize_ = size;
		invCellSize_ = 1.0f / size;
	}

	void UniformGrid::Clear() {
		cells_.clear();
	}

	void UniformGrid::CellRangeFor(const AABB& aabb, CellKey& outMin, CellKey& outMax) const {
		outMin.x = static_cast<int32_t>(std::floor(aabb.min.x * invCellSize_));
		outMin.y = static_cast<int32_t>(std::floor(aabb.min.y * invCellSize_));
		outMin.z = static_cast<int32_t>(std::floor(aabb.min.z * invCellSize_));
		outMax.x = static_cast<int32_t>(std::floor(aabb.max.x * invCellSize_));
		outMax.y = static_cast<int32_t>(std::floor(aabb.max.y * invCellSize_));
		outMax.z = static_cast<int32_t>(std::floor(aabb.max.z * invCellSize_));
	}

	void UniformGrid::Insert(BaseCollider* c, const AABB& aabb) {
		if (!c) return;

		CellKey cmin, cmax;
		CellRangeFor(aabb, cmin, cmax);

		// 異常に大きい AABB は最大セル数を制限してフォールバック
		const int kMaxCellSpan = 32;
		if ((cmax.x - cmin.x) > kMaxCellSpan ||
			(cmax.y - cmin.y) > kMaxCellSpan ||
			(cmax.z - cmin.z) > kMaxCellSpan) {

			// デバッグ用: フォールバック発生を1秒おきにまとめてログ出力
			static int fallbackCount = 0;
			static int frameCounter = 0;
			++fallbackCount;
			if (++frameCounter >= 60) {
				char buf[256];
				sprintf_s(buf,
					"[UniformGrid] (0,0,0)フォールバック: 60フレームで%d回 直近AABB=(%.1f..%.1f, %.1f..%.1f, %.1f..%.1f) span=(%d,%d,%d)\n",
					fallbackCount,
					aabb.min.x, aabb.max.x, aabb.min.y, aabb.max.y, aabb.min.z, aabb.max.z,
					cmax.x - cmin.x, cmax.y - cmin.y, cmax.z - cmin.z);
				Logger(buf);
				fallbackCount = 0;
				frameCounter = 0;
			}

			cells_[{0, 0, 0}].push_back(c);
			return;
		}

		for (int z = cmin.z; z <= cmax.z; ++z) {
			for (int y = cmin.y; y <= cmax.y; ++y) {
				for (int x = cmin.x; x <= cmax.x; ++x) {
					cells_[{x, y, z}].push_back(c);
				}
			}
		}
	}

	void UniformGrid::DrawDebugAroundCamera(InstancedCube* cubes,
		const Vector3& cameraPos, float radius, const Vector4& color) const
	{
		if (!cubes) return;
		if (radius <= 0.0f) return;

		// カメラ周辺の AABB をセル座標に変換
		Vector3 mn{ cameraPos.x - radius, cameraPos.y - radius, cameraPos.z - radius };
		Vector3 mx{ cameraPos.x + radius, cameraPos.y + radius, cameraPos.z + radius };

		int32_t ix0 = static_cast<int32_t>(std::floor(mn.x * invCellSize_));
		int32_t iy0 = static_cast<int32_t>(std::floor(mn.y * invCellSize_));
		int32_t iz0 = static_cast<int32_t>(std::floor(mn.z * invCellSize_));
		int32_t ix1 = static_cast<int32_t>(std::floor(mx.x * invCellSize_));
		int32_t iy1 = static_cast<int32_t>(std::floor(mx.y * invCellSize_));
		int32_t iz1 = static_cast<int32_t>(std::floor(mx.z * invCellSize_));

		// 各セルを 1 つのキューブとして InstancedCube に登録
		for (int32_t z = iz0; z <= iz1; ++z) {
			for (int32_t y = iy0; y <= iy1; ++y) {
				for (int32_t x = ix0; x <= ix1; ++x) {
					Vector3 cellMin{
						static_cast<float>(x) * cellSize_,
						static_cast<float>(y) * cellSize_,
						static_cast<float>(z) * cellSize_
					};
					Vector3 cellMax{
						cellMin.x + cellSize_,
						cellMin.y + cellSize_,
						cellMin.z + cellSize_
					};
					cubes->AddAABB(cellMin, cellMax, color);
				}
			}
		}
	}

	void UniformGrid::QueryPairs(std::vector<std::pair<BaseCollider*, BaseCollider*>>& outPairs) {
		// 同セル内コライダーの全ペアを生成。重複ペアは unordered_set で防ぐ。
		struct PairHash {
			size_t operator()(const std::pair<BaseCollider*, BaseCollider*>& p) const noexcept {
				auto h1 = std::hash<BaseCollider*>{}(p.first);
				auto h2 = std::hash<BaseCollider*>{}(p.second);
				return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
			}
		};
		std::unordered_set<std::pair<BaseCollider*, BaseCollider*>, PairHash> seen;

		outPairs.clear();
		for (auto& [key, list] : cells_) {
			const size_t n = list.size();
			if (n < 2) continue;
			for (size_t i = 0; i < n; ++i) {
				for (size_t j = i + 1; j < n; ++j) {
					BaseCollider* a = list[i];
					BaseCollider* b = list[j];
					if (a > b) std::swap(a, b);
					auto pr = std::make_pair(a, b);
					if (seen.insert(pr).second) {
						outPairs.push_back(pr);
					}
				}
			}
		}
	}

} // namespace YoRigine
