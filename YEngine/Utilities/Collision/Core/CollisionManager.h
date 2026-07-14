#pragma once

// Engine
#include "BaseCollider.h"
#include "Object3D/Object3d.h"
#include "WorldTransform/WorldTransform.h"

// C++
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>

// Math
#include "MathFunc.h"
#include "../Sphere/SphereCollider.h"
#include "../AABB/AABBCollider.h"
#include "../OBB/OBBCollider.h"
#include "../Capsule/CapsuleCollider.h"
#include "../Broad/UniformGrid.h"
#include "CollisionDirection.h"
#include "Intersection/Intersection.h"
#include "Frustum/Frustum.h"

namespace YoRigine {

	// ============================================================
	// 衝突方向のビット列定義
	// ============================================================
	enum HitDirectionFlags {
		HitDirection_None = 0,
		HitDirection_Top = 1 << 0,
		HitDirection_Bottom = 1 << 1,
		HitDirection_Left = 1 << 2,
		HitDirection_Right = 1 << 3,
		HitDirection_Front = 1 << 4,
		HitDirection_Back = 1 << 5,
	};

	using HitDirectionBits = uint32_t;

	// ============================================================
	// コリジョン管理クラス
	// ============================================================
	class CollisionManager {
	public:
		static CollisionManager* GetInstance();

		CollisionManager() = default;
		~CollisionManager();

		// ============================================================
		// 基本関数
		// ============================================================
		void Initialize();
		void Update();
		void Reset();

		// ============================================================
		// 管理操作
		// ============================================================
		void CheckCollisionPair(BaseCollider* a, BaseCollider* b);
		void CheckAllCollisions();
		bool IsColliderInView(const Vector3& position, const Camera* camera);
		void AddCollider(BaseCollider* collider);
		void RemoveCollider(BaseCollider* collider);

		// ── Frustum culling 設定 ──────────────────────────
		// 視錐台外のコライダーを BroadPhase Insert からスキップする。
		// checkOutsideCamera=true のコライダーに対して効く (BaseCollider::IsCheckOutsideCamera())。
		// 既定 false。
		void SetEnableFrustumCulling(bool enable) { enableFrustumCulling_ = enable; }
		bool GetEnableFrustumCulling() const      { return enableFrustumCulling_; }

		// 視錐台の元になる Camera を設定。SetCamera をしていれば自動的に毎フレーム VP から抽出する。
		void SetCullingCamera(Camera* cam) { cullingCamera_ = cam; }
		const std::vector<BaseCollider*>& GetColliders() const { return colliders_; }

		// 視錐台 culling が有効か (enable フラグ + カメラ両方がそろっているか)
		bool IsCullingActive() const { return enableFrustumCulling_ && cullingCamera_; }

		// 指定 AABB が culling 視錐台の外側なら true。culling 無効なら常に false。
		// 個別コライダーの IsCheckOutsideCamera は見ないので呼び出し側でチェックすること。
		// (ObjectManager 等、CollisionManager より早い段階で per-object スキップ判定するために使う)
		bool IsAABBOutsideCullingFrustum(const AABB& aabb);

		// ============================================================
		// レイキャスト判定
		//   ignoreTypeIDs: 旧来用、typeID 一致でスキップ
		//   layerMask:    レイがどの層に反応するか。0xFFFFFFFFu で全層
		// ============================================================
		bool Raycast(const Ray& ray, float maxDistance, RaycastHit* outHit, const std::vector<uint32_t>& ignoreTypeIDs = {});
		bool RaycastMasked(const Ray& ray, float maxDistance, uint32_t layerMask, RaycastHit* outHit);

		// ============================================================
		// 一時オーバーラップクエリ（持続コライダーを作らず「今この瞬間」だけ調べる）
		//   - Enter/Exit・接触猶予・CCD 等の持続状態を一切持たない一回限りの検索。
		//   - 衝撃波/爆発/範囲攻撃のダメージ判定など「一瞬だけ範囲を調べたい」用途向け。
		//     GPUパーティクル1個1個のような大量対象の判定には使わないこと
		//     （設計は Docs/VfxExpansion_Design.md の 5.4 を参照）。
		//   - 実装は colliders_ の線形走査 + AABB近似（ComputeWorldAABB とのSphere-AABB判定）。
		//     プレイヤー/敵/壁程度の登録数（数百程度まで）を想定した簡易版。
		//   layerMask:     候補コライダーの layerBits と AND を取り、0 ならスキップ。0xFFFFFFFFu で全層。
		//   ignoreTypeIDs: typeID が一致するコライダーをスキップ。
		// ============================================================
		std::vector<BaseCollider*> QuerySphere(const Vector3& center, float radius,
			uint32_t layerMask = 0xFFFFFFFFu, const std::vector<uint32_t>& ignoreTypeIDs = {}) const;

		// ============================================================
		// Broad Phase 設定
		// ============================================================
		void SetBroadPhaseCellSize(float size) { grid_.SetCellSize(size); }
		float GetBroadPhaseCellSize() const    { return grid_.GetCellSize(); }

		// Broad Phase グリッドへのアクセサ (デバッグ描画用)
		UniformGrid&       GetBroadPhaseGrid()       { return grid_; }
		const UniformGrid& GetBroadPhaseGrid() const { return grid_; }
		bool IsIterating() const { return isIterating_; }

		// ============================================================
		// 反復押し戻し回数 (3-4 推奨。1 は単純解決、0 で押し戻し無効)
		// ============================================================
		void SetResolveIterations(int n) { resolveIterations_ = (n < 0 ? 0 : n); }
		int  GetResolveIterations() const { return resolveIterations_; }

		// ============================================================
		// 接触の Exit 猶予フレーム数 (スティッキー接触)
		//   一度接触したペアは、連続でこのフレーム数を超えて離れない限り
		//   Exit を発火しない。押し戻し(ResolveContacts)が narrow 判定を
		//   1フレームだけ no-hit に揺らして Enter が多重発火する問題を防ぐ。
		//   0 で従来通り (猶予なし)。既定 2。
		// ============================================================
		void SetContactExitGraceFrames(int n) { contactExitGraceFrames_ = (n < 0 ? 0 : n); }
		int  GetContactExitGraceFrames() const { return contactExitGraceFrames_; }

		// ============================================================
		// 形状からワールドAABBを計算
		// ============================================================
		static AABB ComputeWorldAABB(BaseCollider* c);

		// ============================================================
		// ヒット方向判定用ユーティリティ
		// ============================================================
		static HitDirection ConvertVectorToHitDirection(const Vector3& dir);
		static HitDirection InverseHitDirection(HitDirection hitdirection);
		static HitDirection GetSelfLocalHitDirection(BaseCollider* self, BaseCollider* other);
		static HitDirectionBits GetSelfLocalHitDirectionFlags(BaseCollider* self, BaseCollider* other, float threshold);
		static HitDirectionBits GetSelfLocalHitDirectionsSimple(BaseCollider* self, BaseCollider* other);

	private:
		CollisionManager(const CollisionManager&) = delete;
		CollisionManager& operator=(const CollisionManager&) = delete;

	private:
		// 形状ベースのディスパッチ。dynamic_cast を経由しない。
		// (a, b) は事前に (a.shape <= b.shape) になるよう CheckCollisionPair で並び替える。
		// この関数は「形状的にAがBを押し戻すべき法線」を返す前提で書く。
		bool DispatchShapePair(BaseCollider* a, BaseCollider* b, CollisionResult* outResult);
		bool DispatchRay(const Ray& ray, BaseCollider* c, RaycastHit* outHit);

		// Broad Phase 候補ペアに対して反復押し戻しを行う (質量比 + 2段累積)
		void ResolveContacts(const std::vector<std::pair<BaseCollider*, BaseCollider*>>& pairs,
			int iterations);

		// CCD: 前フレーム位置→現在位置を Raycast でスイープしてトンネリングを防ぐ
		void SweepCCDColliders();

		std::vector<BaseCollider*> colliders_;

		// ペアキーをハッシュ化して O(1) 平均で検索する
		struct PairHash {
			size_t operator()(const std::pair<BaseCollider*, BaseCollider*>& p) const noexcept {
				auto h1 = std::hash<BaseCollider*>{}(p.first);
				auto h2 = std::hash<BaseCollider*>{}(p.second);
				return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
			}
		};
		// 接触中ペア → 連続で no-hit だったフレーム数 (missStreak)。
		// hit のたびに 0 リセット、no-hit のたびに +1。
		// missStreak が contactExitGraceFrames_ を超えた時点で Exit を発火し除去する。
		std::unordered_map<std::pair<BaseCollider*, BaseCollider*>, int, PairHash> collidingPairs_;

		// Broad Phase 用 Uniform Grid
		UniformGrid grid_;
		std::vector<std::pair<BaseCollider*, BaseCollider*>> broadPhasePairsScratch_;

		// 反復押し戻し回数 (0 で無効)
		int resolveIterations_ = 3;

		// 接触の Exit 猶予フレーム数 (スティッキー接触。0 で従来通り)
		int contactExitGraceFrames_ = 2;

		// Frustum culling
		bool    enableFrustumCulling_ = false;
		Camera* cullingCamera_        = nullptr;

		// 走査中 (CheckAllCollisions 中) に Add/Remove が来た場合に保留する
		bool isIterating_ = false;
		std::vector<BaseCollider*> pendingAdds_;
		std::vector<BaseCollider*> pendingRemoves_;

		void FlushPending();
		void DoRemove(BaseCollider* c);

		bool isDrawCollider_ = false;
	};
}
