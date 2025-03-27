#include "CollisionManager.h"
// C++
#include <assert.h>
#include <iostream>

// Engine
#include "GlobalVariables.h"
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

void CollisionManager::UpdateCollision()
{
	// ワールドトランスフォームの更新
	UpdateWorldTransform();
	// 衝突判定
	CheckAllCollisions();
	

}
void CollisionManager::UpdateWorldTransform() {

	//ApplyGlobalVariables();
	DebugImGui();
	// 非表示なら抜ける
	if (!isDrawCollider_) {
		return;
	}
	// 全てのコライダーについて
	for (Collider* collider : colliders_) {
		// 更新
		collider->UpdateWorldTransform();
	}



}
void CollisionManager::Draw() {
	// 非表示なら抜ける
	if (!isDrawCollider_) {
		return;
	}
	// 全てのコライダーについて
	for (Collider* collider : colliders_) {
		// 描画
		//collider->Draw(.get());
	}
}


void CollisionManager::DebugImGui()
{
#ifdef _DEBUG
	if (ImGui::Begin("Collision Manager Debug")) {
		// 登録されたコライダーの数を表示
		ImGui::Text("Number of Colliders: %zu", colliders_.size());

		// 各コライダーの情報を表示
		int index = 0;
		for (auto* collider : colliders_) {
			ImGui::PushID(index);
			if (ImGui::CollapsingHeader(("Collider " + std::to_string(index)).c_str())) {
				ImGui::Text("Type ID: %u", collider->GetTypeID());
				Vector3 pos = collider->GetCenterPosition();
				ImGui::Text("Position: (%.2f,%.2f,%.2f)", pos.x,pos.y,pos.z);
				float radius = collider->GetRadiusFloat();
				ImGui::Text("Radius: (%.2f)", radius);
			}
			ImGui::PopID();
			++index;
		}
	}
	ImGui::End();
#endif
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
	if (distance <= (colliderA->GetRadiusFloat() + colliderB->GetRadiusFloat())) {
		// コライダーAの衝突時コールバックを呼び出す
		colliderA->OnCollision(colliderB);
		// コライダーBの衝突時コールバックを呼び出す
		colliderB->OnCollision(colliderA);
	}
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


//void CollisionManager::CheckCollisionPair(Collider* colliderA, Collider* colliderB) {
//	// 両方のIDが同じかどちらかがNoneだった場合早期リターン
//	if (colliderA->GetTypeID() == colliderB->GetTypeID() ||
//		colliderA->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kNone) ||
//		colliderB->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kNone)) {
//		return;
//	}
//
//	// OBBデータを取得（中心位置、半径、軸ベクトル）
//	Vector3 centerA = colliderA->GetCenterPosition();
//	Vector3 centerB = colliderB->GetCenterPosition();
//
//	Matrix4x4 matA = colliderA->GetWorldMatrix();
//	Matrix4x4 matB = colliderB->GetWorldMatrix();
//
//	Vector3 axesA[3] = {
//		Vector3(matA.m[0][0], matA.m[0][1], matA.m[0][2]),
//		Vector3(matA.m[1][0], matA.m[1][1], matA.m[1][2]),
//		Vector3(matA.m[2][0], matA.m[2][1], matA.m[2][2])
//	};
//
//	Vector3 axesB[3] = {
//		Vector3(matB.m[0][0], matB.m[0][1], matB.m[0][2]),
//		Vector3(matB.m[1][0], matB.m[1][1], matB.m[1][2]),
//		Vector3(matB.m[2][0], matB.m[2][1], matB.m[2][2])
//	};
//
//	Vector3 halfExtentsA = colliderA->GetRadiusVector3(); // Aの半径
//	Vector3 halfExtentsB = colliderB->GetRadiusVector3(); // Bの半径
//
//	// 中心間ベクトル
//	Vector3 T = centerB - centerA;
//
//	// 15軸で判定（3+3+9）
//	for (int i = 0; i < 3; ++i) {
//		// Aの軸
//		if (!TestAxis(axesA[i], T, axesA, halfExtentsA, axesB, halfExtentsB)) return;
//		// Bの軸
//		if (!TestAxis(axesB[i], T, axesA, halfExtentsA, axesB, halfExtentsB)) return;
//	}
//
//	// 交差チェック成功時
//	for (int i = 0; i < 3; ++i) {
//		for (int j = 0; j < 3; ++j) {
//			Vector3 axis = Cross(axesA[i], axesB[j]);
//			if (!TestAxis(axis, T, axesA, halfExtentsA, axesB, halfExtentsB)) return;
//		}
//	}
//
//	// 衝突が検出された場合
//	colliderA->OnCollision(colliderB);
//	colliderB->OnCollision(colliderA);
//
//}


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

void CollisionManager::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	const char* groupName = "Collider";
	isDrawCollider_ = globalVariables->GetBoolValue(groupName, "Collider");

}

// 分離軸テスト関数
// 分離軸テスト関数（TestAxis）
bool CollisionManager::TestAxis(
	const Vector3& axis,
	const Vector3& T,
	const Vector3 axesA[3],
	const Vector3& halfExtentsA,
	const Vector3 axesB[3],
	const Vector3& halfExtentsB
) {
	if (Length(axis) < 0.0001f) return true; // ほぼ0ベクトルは無視

	// Aの投影半径を計算
	float radiusA =
		fabs(Dot(axesA[0], axis)) * halfExtentsA.x +
		fabs(Dot(axesA[1], axis)) * halfExtentsA.y +
		fabs(Dot(axesA[2], axis)) * halfExtentsA.z;

	// Bの投影半径を計算
	float radiusB =
		fabs(Dot(axesB[0], axis)) * halfExtentsB.x +
		fabs(Dot(axesB[1], axis)) * halfExtentsB.y +
		fabs(Dot(axesB[2], axis)) * halfExtentsB.z;

	// Tを軸に投影した距離
	float distance = fabs(Dot(T, axis));

	// 投影距離が半径の合計以下なら衝突
	return distance <= (radiusA + radiusB);
}