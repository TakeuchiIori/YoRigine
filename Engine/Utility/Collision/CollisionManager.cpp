#include "CollisionManager.h"
// C++
#include <assert.h>
#include <iostream>

// Engine
#include "Loaders./Model/ModelManager.h"
#include "CollisionTypeIdDef.h"

// C++
#include "MathFunc.h"
#include <algorithm>


#ifdef _DEBUG
#include "imgui.h"
#endif // _DEBUG




CollisionManager* CollisionManager::GetInstance()
{
	static CollisionManager instance;
	return &instance;
}



bool Collision::Check(const SphereCollider* a, const SphereCollider* b)
{
	Vector3 diff = b->GetCenterPosition() - a->GetCenterPosition();
	float distSq = Length(diff);
	float radiusSum = a->GetRadius() + b->GetRadius();
	return distSq <= radiusSum * radiusSum;
}

bool Collision::Check(const SphereCollider* sphere, const AABBCollider* aabb)
{
	Vector3 closest = std::clamp(sphere->GetCenterPosition(), aabb->GetAABB().min, aabb->GetAABB().max);
	Vector3 diff = closest - sphere->GetCenterPosition();
	return Length(diff) <= sphere->GetRadius() * sphere->GetRadius();
}

bool Collision::Check(const SphereCollider* sphere, const OBBCollider* obb)
{
	const OBB& ob = obb->GetOBB();
	Vector3 localPos = ob.rotation.Inverse() * (sphere->GetCenterPosition() - ob.center);
	Vector3 clamped = Clamp(localPos, -ob.size * 0.5f, ob.size * 0.5f);
	Vector3 closest = ob.center + ob.rotation * clamped;
	Vector3 diff = closest - sphere->GetCenterPosition();
	return Length(diff) <= sphere->GetRadius() * sphere->GetRadius();
}

bool Collision::Check(const AABBCollider* a, const AABBCollider* b)
{
	const AABB& aa = a->GetAABB();
	const AABB& bb = b->GetAABB();
	return (aa.min.x <= bb.max.x && aa.max.x >= bb.min.x) &&
		(aa.min.y <= bb.max.y && aa.max.y >= bb.min.y) &&
		(aa.min.z <= bb.max.z && aa.max.z >= bb.min.z);
}

bool Collision::Check(const OBB& obbA, const OBB& obbB)
{
	Matrix4x4 matA = MakeRotateMatrix(obbA.rotation);
	Matrix4x4 matB = MakeRotateMatrix(obbB.rotation);

	Vector3 axesA[3] = {
		{ matA.m[0][0], matA.m[1][0], matA.m[2][0] },
		{ matA.m[0][1], matA.m[1][1], matA.m[2][1] },
		{ matA.m[0][2], matA.m[1][2], matA.m[2][2] }
	};

	Vector3 axesB[3] = {
		{ matB.m[0][0], matB.m[1][0], matB.m[2][0] },
		{ matB.m[0][1], matB.m[1][1], matB.m[2][1] },
		{ matB.m[0][2], matB.m[1][2], matB.m[2][2] }
	};

	Vector3 distance = obbB.center - obbA.center;

	for (int i = 0; i < 3; ++i) {
		Vector3 axis = axesA[i];
		if (!axis.IsZero()) {
			float projA = obbA.size.x * 0.5f * fabs(Dot(axesA[0], axis)) +
				obbA.size.y * 0.5f * fabs(Dot(axesA[1], axis)) +
				obbA.size.z * 0.5f * fabs(Dot(axesA[2], axis));
			float projB = obbB.size.x * 0.5f * fabs(Dot(axesB[0], axis)) +
				obbB.size.y * 0.5f * fabs(Dot(axesB[1], axis)) +
				obbB.size.z * 0.5f * fabs(Dot(axesB[2], axis));
			if (fabs(Dot(distance, axis)) > projA + projB) return false;
		}
	}

	for (int i = 0; i < 3; ++i) {
		Vector3 axis = axesB[i];
		if (!axis.IsZero()) {
			float projA = obbA.size.x * 0.5f * fabs(Dot(axesA[0], axis)) +
				obbA.size.y * 0.5f * fabs(Dot(axesA[1], axis)) +
				obbA.size.z * 0.5f * fabs(Dot(axesA[2], axis));
			float projB = obbB.size.x * 0.5f * fabs(Dot(axesB[0], axis)) +
				obbB.size.y * 0.5f * fabs(Dot(axesB[1], axis)) +
				obbB.size.z * 0.5f * fabs(Dot(axesB[2], axis));
			if (fabs(Dot(distance, axis)) > projA + projB) return false;
		}
	}

	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			Vector3 axis = Normalize(Cross(axesA[i], axesB[j]));
			if (!axis.IsZero()) {
				float projA = obbA.size.x * 0.5f * fabs(Dot(axesA[0], axis)) +
					obbA.size.y * 0.5f * fabs(Dot(axesA[1], axis)) +
					obbA.size.z * 0.5f * fabs(Dot(axesA[2], axis));
				float projB = obbB.size.x * 0.5f * fabs(Dot(axesB[0], axis)) +
					obbB.size.y * 0.5f * fabs(Dot(axesB[1], axis)) +
					obbB.size.z * 0.5f * fabs(Dot(axesB[2], axis));
				if (fabs(Dot(distance, axis)) > projA + projB) return false;
			}
		}
	}

	return true;
}

bool Collision::Check(const AABBCollider* aabb, const OBBCollider* obb)
{
	OBB aabbAsOBB;
	aabbAsOBB.center = (aabb->GetAABB().min + aabb->GetAABB().max) * 0.5f;
	aabbAsOBB.size = aabb->GetAABB().max - aabb->GetAABB().min;
	aabbAsOBB.rotation = Quaternion::Identity();
	return Collision::Check(aabbAsOBB, obb->GetOBB());

}

bool Collision::Check(const OBBCollider* a, const OBBCollider* b)
{
	return Check(a->GetOBB(), b->GetOBB());
}




void CollisionManager::Initialize() {
	isDrawCollider_ = false;
}

void CollisionManager::Update()
{
	CheckAllCollisions();

}



void CollisionManager::Reset() {
	// リストを空っぽにする
	colliders_.clear();

}

void CollisionManager::CheckCollisionPair(Collider* colliderA, Collider* colliderB) {
	// コライダーAの座標を取得
	Vector3 PosA = colliderA->GetCenterPosition();
	// コライダーBの座標を取得
	Vector3 PosB = colliderB->GetCenterPosition();
	// 座標の差分ベクトル
	Vector3 subtract = PosB - PosA;
	// 座標AとBの距離を求める
	float distance = Length(subtract);

	// 球と球の交差判定
	//if (distance <= (colliderA->GetRadius() + colliderB->GetRadius())) {
	//	// コライダーAの衝突時コールバックを呼び出す
	//	colliderA->OnCollision(colliderB);
	//	// コライダーBの衝突時コールバックを呼び出す
	//	colliderB->OnCollision(colliderA);
	//}
}

void CollisionManager::CheckAllCollisions() {
	// リスト内のペアを総当たり
	std::list<Collider*>::iterator itrA = colliders_.begin();
	for (; itrA != colliders_.end(); ++itrA) {
		Collider* colliderA = *itrA;

		// イテレーターBはイテレーターAの次の要素から回す（重複判定を回避）
		std::list<Collider*>::iterator itrB = itrA;
		itrB++;

		for (; itrB != colliders_.end(); ++itrB) {
			Collider* colliderB = *itrB;

			// ペアの当たり判定
			CheckCollisionPair(colliderA, colliderB);
		}
	}
}



void CollisionManager::AddCollider(Collider* collider) {
	if (!collider) return;
	colliders_.push_back(collider);
	std::cout << "Collider added: " << collider->GetTypeID() << std::endl;
}

void CollisionManager::RemoveCollider(Collider* collider)
{
	if (!collider) return;
	colliders_.remove(collider);
	std::cout << "Collider removed: " << collider->GetTypeID() << std::endl;
}
