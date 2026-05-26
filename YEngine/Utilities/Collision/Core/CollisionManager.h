#pragma once

// Engine
#include "BaseCollider.h"
#include "Object3D/Object3d.h"
#include "WorldTransform/WorldTransform.h"

// C++
#include <list>
#include <memory>
#include <set>

// Math
#include "MathFunc.h"
#include "../Sphere/SphereCollider.h"
#include "../AABB/AABBCollider.h"
#include "../OBB/OBBCollider.h"
#include "CollisionDirection.h"
#include "Intersection/Intersection.h"

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
		const std::list<BaseCollider*>& GetColliders() const { return colliders_; }

		// ============================================================
		// レイキャスト判定
		// ============================================================
		bool Raycast(const Ray& ray, float maxDistance, RaycastHit* outHit, const std::vector<uint32_t>& ignoreTypeIDs = {});

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
		std::list<BaseCollider*> colliders_;
		std::set<std::pair<BaseCollider*, BaseCollider*>> collidingPairs_;
		bool isDrawCollider_ = false;
	};
}