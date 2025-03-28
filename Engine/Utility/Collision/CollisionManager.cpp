#include "CollisionManager.h"
// C++
#include <assert.h>
#include <iostream>

// Engine
#include "Loaders./Model/ModelManager.h"
#include "CollisionTypeIdDef.h"

// C++
#include "MathFunc.h"

#ifdef _DEBUG
#include "imgui.h"
#endif // _DEBUG




CollisionManager* CollisionManager::GetInstance()
{
	static CollisionManager instance;
	return &instance;
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



