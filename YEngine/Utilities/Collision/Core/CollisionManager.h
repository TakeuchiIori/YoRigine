#pragma once

// Engine
#include "BaseCollider.h"
#include "Object3D/Object3d.h"
#include "WorldTransform./WorldTransform.h"

// C++
#include <list>
#include <memory>
#include <set>

// Math
#include "MathFunc.h"

// Collider
#include "../Sphere/SphereCollider.h"
#include "../AABB/AABBCollider.h"
#include "../OBB/OBBCollider.h"
#include "CollisionDirection.h"

namespace YoRigine {
	///************************* ヒット方向定義 *************************///

	// 衝突方向のビット列定義
	enum HitDirectionFlags {
		HitDirection_None = 0,
		HitDirection_Top = 1 << 0,
		HitDirection_Bottom = 1 << 1,
		HitDirection_Left = 1 << 2,
		HitDirection_Right = 1 << 3,
		HitDirection_Front = 1 << 4,
		HitDirection_Back = 1 << 5,
	};

	// 複数方向を保持するための型
	using HitDirectionBits = uint32_t;

	///************************* 衝突判定関連関数 *************************///

	// すべての衝突チェック処理と方向判定処理をまとめた名前空間
	namespace Collision {

		///************************* 衝突チェック *************************///

		// Sphere - Sphere
		bool Check(const SphereCollider* a, const SphereCollider* b);

		// Sphere - AABB
		bool Check(const SphereCollider* sphere, const AABBCollider* aabb);

		// Sphere - OBB
		bool Check(const SphereCollider* sphere, const OBBCollider* obb);

		// AABB - AABB
		bool Check(const AABBCollider* a, const AABBCollider* b);

		// OBB - OBB（データ構造版）
		bool Check(const OBB& obbA, const OBB& obbB);

		// AABB - OBB
		bool Check(const AABBCollider* aabb, const OBBCollider* obb);

		// OBB - OBB（コライダー版）
		bool Check(const OBBCollider* a, const OBBCollider* b);

		// Base - Base（汎用）
		bool Check(BaseCollider* a, BaseCollider* b);

		///************************* 衝突方向チェック *************************///

		// AABB - AABB
		bool CheckHitDirection(const AABB& a, const AABB& b, HitDirection* hitDirection);

		// AABB - OBB
		bool CheckHitDirection(const AABB& aabb, const OBB& obb, HitDirection* hitDirection);

		// OBB - OBB
		bool CheckHitDirection(const OBB& obbA, const OBB& obbB, HitDirection* hitDirection);

		// ベクトルから方向へ変換
		HitDirection ConvertVectorToHitDirection(const Vector3& dir);

		// 方向の反転
		HitDirection InverseHitDirection(HitDirection hitdirection);

		// 自分基準での衝突方向取得
		HitDirection GetSelfLocalHitDirection(BaseCollider* self, BaseCollider* other);

		// 一定閾値で方向ビットを取得
		HitDirectionBits GetSelfLocalHitDirectionFlags(BaseCollider* self, BaseCollider* other, float threshold);

		// シンプルな方向ビット取得
		HitDirectionBits GetSelfLocalHitDirectionsSimple(BaseCollider* self, BaseCollider* other);
	}

	///************************* コリジョン管理クラス *************************///

	// コライダーを一括管理し、衝突判定とコールバックを制御するzz
	class CollisionManager {
	public:
		///************************* シングルトン *************************///

		static CollisionManager* GetInstance();

		CollisionManager() = default;
		~CollisionManager();

	public:
		///************************* 基本関数 *************************///

		// 初期化
		void Initialize();

		// 更新（すべての当たり判定処理を実行）
		void Update();

	public:
		///************************* 管理操作 *************************///

		// 登録情報を全リセット
		void Reset();

		// コライダー2つの衝突チェックとコールバック呼び出し
		void CheckCollisionPair(BaseCollider* a, BaseCollider* b);

		// すべてのコライダー間の衝突チェック
		void CheckAllCollisions();

		// カメラ範囲内かを確認（非表示領域のコライダーをスキップ）
		bool IsColliderInView(const Vector3& position, const Camera* camera);

		// コライダーをリストに追加
		void AddCollider(BaseCollider* collider);

		// コライダーをリストから削除
		void RemoveCollider(BaseCollider* collider);

	private:
		///************************* コピー禁止 *************************///

		CollisionManager(const CollisionManager&) = delete;
		CollisionManager& operator=(const CollisionManager&) = delete;

	private:
		///************************* メンバ変数 *************************///

		// 登録中のすべてのコライダー
		std::list<BaseCollider*> colliders_;

		// 現在衝突中のペアを記録（Enter/Exit検知用）
		std::set<std::pair<BaseCollider*, BaseCollider*>> collidingPairs_;

		// デバッグ描画フラグ
		bool isDrawCollider_ = false;
	};
}