#include "CollisionManager.h"
#include <assert.h>
#include <iostream>
#include "CollisionTypeIdDef.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif // _DEBUG

namespace YoRigine {

	CollisionManager* CollisionManager::GetInstance() {
		static CollisionManager instance;
		return &instance;
	}

	CollisionManager::~CollisionManager() {
		Reset();
	}

	void CollisionManager::Initialize() {
		isDrawCollider_ = false;
	}

	void CollisionManager::Update() {
		CheckAllCollisions();
	}

	void CollisionManager::Reset() {
		colliders_.clear();
		collidingPairs_.clear();
	}

	// ============================================================
	// 2つのコライダーの当たり判定とイベント呼び出し、押し戻し処理
	// ============================================================
	void CollisionManager::CheckCollisionPair(BaseCollider* a, BaseCollider* b) {
		if (!a || !b) return;
		auto key = std::minmax(a, b);
		bool wasColliding = collidingPairs_.contains(key);

		CollisionResult res;

		// ============================================================
		// 形状ごとに Intersection::IsCollision を呼び出す
		// ============================================================
		if (auto sa = dynamic_cast<SphereCollider*>(a)) {
			Sphere sphereA = { sa->GetCenterPosition(), sa->GetRadius() };
			if (auto sb = dynamic_cast<SphereCollider*>(b)) {
				Sphere sphereB = { sb->GetCenterPosition(), sb->GetRadius() };
				Intersection::IsCollision(sphereA, sphereB, &res);
			}
			else if (auto ob = dynamic_cast<OBBCollider*>(b)) {
				Intersection::IsCollision(sphereA, ob->GetOBB(), &res);
			}
			else if (auto ab = dynamic_cast<AABBCollider*>(b)) {
				Intersection::IsCollision(sphereA, ab->GetAABB(), &res);
			}
		}
		else if (auto oa = dynamic_cast<OBBCollider*>(a)) {
			if (auto sb = dynamic_cast<SphereCollider*>(b)) {
				Sphere sphereB = { sb->GetCenterPosition(), sb->GetRadius() };
				Intersection::IsCollision(sphereB, oa->GetOBB(), &res);
				res.normal = res.normal * -1.0f; // 基準が逆になるので反転
			}
			else if (auto ob = dynamic_cast<OBBCollider*>(b)) {
				Intersection::IsCollision(oa->GetOBB(), ob->GetOBB(), &res);
			}
			else if (auto ab = dynamic_cast<AABBCollider*>(b)) {
				Intersection::IsCollision(ab->GetAABB(), oa->GetOBB(), &res);
				res.normal = res.normal * -1.0f; // 基準が逆になるので反転
			}
		}
		else if (auto aa = dynamic_cast<AABBCollider*>(a)) {
			if (auto sb = dynamic_cast<SphereCollider*>(b)) {
				Sphere sphereB = { sb->GetCenterPosition(), sb->GetRadius() };
				Intersection::IsCollision(sphereB, aa->GetAABB(), &res);
				res.normal = res.normal * -1.0f; // 基準が逆になるので反転
			}
			else if (auto ob = dynamic_cast<OBBCollider*>(b)) {
				Intersection::IsCollision(aa->GetAABB(), ob->GetOBB(), &res);
			}
			else if (auto ab = dynamic_cast<AABBCollider*>(b)) {
				Intersection::IsCollision(aa->GetAABB(), ab->GetAABB(), &res);
			}
		}

		// ============================================================
		// 押し戻しの処理
		// ============================================================
		if (res.isHit && res.penetrationDepth > 0.0f) {
			if (a->GetEnablePenetration() && b->GetEnablePenetration()) {
				WorldTransform* wtA = a->GetWT();
				WorldTransform* wtB = b->GetWT();
				bool staticA = a->GetIsStatic();
				bool staticB = b->GetIsStatic();

				if (wtA && wtB) {
					if (!staticA && !staticB) {
						Vector3 velA = a->GetVelocity();
						Vector3 velB = b->GetVelocity();
						float speedSqA = velA.x * velA.x + velA.y * velA.y + velA.z * velA.z;
						float speedSqB = velB.x * velB.x + velB.y * velB.y + velB.z * velB.z;

						float ratioA = 0.5f;
						float ratioB = 0.5f;

						if (speedSqA > 0.00001f || speedSqB > 0.00001f) {
							ratioA = speedSqA / (speedSqA + speedSqB);
							ratioB = speedSqB / (speedSqA + speedSqB);
						}
						else {
							float massA = a->GetMass();
							float massB = b->GetMass();
							float totalMass = massA + massB;
							if (totalMass > 0.0f) {
								ratioA = massB / totalMass;
								ratioB = massA / totalMass;
							}
						}

						wtA->translate_ += res.normal * (res.penetrationDepth * ratioA);
						wtB->translate_ -= res.normal * (res.penetrationDepth * ratioB);

						wtA->UpdateMatrix();
						wtB->UpdateMatrix();
						a->Update();
						b->Update();
					}
				}
			}
		}

		// ============================================================
		// イベント呼び出し
		// ============================================================
		if (res.isHit) {
			if (!wasColliding) {
				a->CallOnEnterCollision(b);
				b->CallOnEnterCollision(a);
				collidingPairs_.insert(key);
			}
			a->CallOnCollision(b);
			b->CallOnCollision(a);
		}
		else {
			if (wasColliding) {
				a->CallOnExitCollision(b);
				b->CallOnExitCollision(a);
				collidingPairs_.erase(key);
			}
		}
	}

	// ============================================================
	// 全コライダーの当たり判定チェック
	// ============================================================
	void CollisionManager::CheckAllCollisions() {
		auto itrA = colliders_.begin();
		for (; itrA != colliders_.end(); ++itrA) {
			BaseCollider* colliderA = *itrA;
			if (colliderA->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kNone)) continue;
			if (!colliderA || !colliderA->GetIsActive() || !colliderA->IsCollisionEnabled()) continue;

			auto itrB = itrA;
			itrB++;

			for (; itrB != colliders_.end(); ++itrB) {
				BaseCollider* colliderB = *itrB;
				if (colliderB->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kNone)) continue;
				if (colliderA == colliderB) continue;
				if (!colliderB || !colliderB->GetIsActive() || !colliderB->IsCollisionEnabled()) continue;

				CheckCollisionPair(colliderA, colliderB);
			}
		}
	}

	// ============================================================
	// すべてのコライダーに対するレイキャスト
	// ============================================================

	bool CollisionManager::IsColliderInView(const Vector3& position, const Camera* camera) {
		Vector3 clipPos = Transform(position, camera->GetViewProjectionMatrix());
		return (clipPos.x >= -1.0f && clipPos.x <= 1.0f &&
			clipPos.y >= -1.0f && clipPos.y <= 1.0f &&
			clipPos.z >= 0.0f && clipPos.z <= 1.0f);
	}

	void CollisionManager::AddCollider(BaseCollider* collider) {
		if (!collider) return;
		colliders_.push_back(collider);
		std::cout << "BaseCollider added: " << collider->GetTypeID() << std::endl;
	}

	void CollisionManager::RemoveCollider(BaseCollider* collider) {
		if (!collider) return;
		colliders_.remove(collider);
		std::cout << "BaseCollider removed: " << collider->GetTypeID() << std::endl;
	}

	bool CollisionManager::Raycast(const Ray& ray, float maxDistance, RaycastHit* outHit, const std::vector<uint32_t>& ignoreTypeIDs)
	{
		bool hitAnything = false;
		float closestDistance = maxDistance;
		RaycastHit tempHit;

		for (BaseCollider* collider : colliders_) {
			if (!collider || !collider->GetIsActive() || !collider->IsCollisionEnabled()) continue;
			// 無視するタイプIDと一致するコライダーはスキップ
			if (!ignoreTypeIDs.empty()) {
				auto it = std::find(ignoreTypeIDs.begin(), ignoreTypeIDs.end(), collider->GetTypeID());
				if (it != ignoreTypeIDs.end()) continue; // リストにあったら無視！
			}

			bool isHit = false;

			if (auto sc = dynamic_cast<SphereCollider*>(collider)) {
				Sphere sphere = { sc->GetCenterPosition(), sc->GetRadius() };
				isHit = Intersection::IsCollision(ray, sphere, &tempHit);
			}
			else if (auto ob = dynamic_cast<OBBCollider*>(collider)) {
				isHit = Intersection::IsCollision(ray, ob->GetOBB(), &tempHit);
			}
			else if (auto ab = dynamic_cast<AABBCollider*>(collider)) {
				isHit = Intersection::IsCollision(ray, ab->GetAABB(), &tempHit);
			}

			// 近すぎるので強制的に無視して次へ
			if (isHit && tempHit.distance <= 0.001f) {
				continue;
			}

			// 最も手前で当たった情報を更新
			if (isHit && tempHit.distance <= closestDistance) {
				closestDistance = tempHit.distance;
				if (outHit) *outHit = tempHit;
				hitAnything = true;
			}
		}

		return hitAnything;
	}

	// ============================================================
	// ヒット方向判定用ユーティリティ
	// ============================================================
	HitDirection CollisionManager::ConvertVectorToHitDirection(const Vector3& dir) {
		if (fabs(dir.x) > fabs(dir.y) && fabs(dir.x) > fabs(dir.z)) {
			return dir.x > 0 ? HitDirection::Right : HitDirection::Left;
		}
		else if (fabs(dir.y) > fabs(dir.z)) {
			return dir.y > 0 ? HitDirection::Top : HitDirection::Bottom;
		}
		else {
			return dir.z > 0 ? HitDirection::Front : HitDirection::Back;
		}
	}

	HitDirection CollisionManager::InverseHitDirection(HitDirection hitdirection) {
		switch (hitdirection) {
		case HitDirection::Top: return HitDirection::Bottom;
		case HitDirection::Bottom: return HitDirection::Top;
		case HitDirection::Left: return HitDirection::Right;
		case HitDirection::Right: return HitDirection::Left;
		case HitDirection::Front: return HitDirection::Back;
		case HitDirection::Back: return HitDirection::Front;
		default: return HitDirection::None;
		}
	}

	HitDirection CollisionManager::GetSelfLocalHitDirection(BaseCollider* self, BaseCollider* other) {
		Vector3 toOther = Normalize(other->GetCenterPosition() - self->GetCenterPosition());
		const Matrix4x4& mat = self->GetWorldTransform().matWorld_;
		Vector3 up = { mat.m[1][0], mat.m[1][1], mat.m[1][2] };
		Vector3 right = { mat.m[0][0], mat.m[0][1], mat.m[0][2] };
		Vector3 forward = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };

		struct DirDot { HitDirection dir; float dot; };
		std::vector<DirDot> dots = {
			{ HitDirection::Top,    Dot(toOther, up) },
			{ HitDirection::Bottom, Dot(toOther, up * -1.0f) },
			{ HitDirection::Right,  Dot(toOther, right) },
			{ HitDirection::Left,   Dot(toOther, right * -1.0f) },
			{ HitDirection::Front,  Dot(toOther, forward) },
			{ HitDirection::Back,   Dot(toOther, forward * -1.0f) },
		};

		const float threshold = 0.5f;
		for (const auto& d : dots) {
			if (d.dot >= threshold) return d.dir;
		}
		return HitDirection::None;
	}

	HitDirectionBits CollisionManager::GetSelfLocalHitDirectionFlags(BaseCollider* self, BaseCollider* other, float threshold) {
		HitDirectionBits flags = HitDirection_None;
		Vector3 toOther = Normalize(other->GetCenterPosition() - self->GetCenterPosition());
		const Matrix4x4& mat = self->GetWorldTransform().matWorld_;
		Vector3 up = { mat.m[1][0], mat.m[1][1], mat.m[1][2] };
		Vector3 right = { mat.m[0][0], mat.m[0][1], mat.m[0][2] };
		Vector3 forward = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };

		if (Dot(toOther, up) >= threshold)     flags |= HitDirection_Top;
		if (Dot(toOther, up * -1.0f) >= threshold)    flags |= HitDirection_Bottom;
		if (Dot(toOther, right) >= threshold)  flags |= HitDirection_Right;
		if (Dot(toOther, right * -1.0f) >= threshold) flags |= HitDirection_Left;
		if (Dot(toOther, forward) >= threshold) flags |= HitDirection_Front;
		if (Dot(toOther, forward * -1.0f) >= threshold) flags |= HitDirection_Back;

		return flags;
	}

	HitDirectionBits CollisionManager::GetSelfLocalHitDirectionsSimple(BaseCollider* self, BaseCollider* other) {
		HitDirectionBits flags = HitDirection_None;
		Vector3 toOther = Normalize(other->GetCenterPosition() - self->GetCenterPosition());
		const Matrix4x4& mat = self->GetWorldTransform().matWorld_;
		Vector3 up = { mat.m[1][0], mat.m[1][1], mat.m[1][2] };
		Vector3 right = { mat.m[0][0], mat.m[0][1], mat.m[0][2] };
		Vector3 forward = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };

		if (Dot(toOther, up) > 0.0f)         flags |= HitDirection_Top;
		if (Dot(toOther, up * -1.0f) > 0.0f) flags |= HitDirection_Bottom;
		if (Dot(toOther, right) > 0.0f)      flags |= HitDirection_Right;
		if (Dot(toOther, right * -1.0f) > 0.0f) flags |= HitDirection_Left;
		if (Dot(toOther, forward) > 0.0f)    flags |= HitDirection_Front;
		if (Dot(toOther, forward * -1.0f) > 0.0f) flags |= HitDirection_Back;

		return flags;
	}
}