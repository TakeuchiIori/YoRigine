#pragma once
// Engine
#include "Collider.h"
#include "Object3D/Object3d.h"
#include "WorldTransform./WorldTransform.h"

// C++
#include <list>
#include <memory>

#include "MathFunc.h"

class CollisionManager {
public: // 基本的な関数

	/// <summary>
    /// シングルトンインスタンスの取得
    /// </summary>
	static CollisionManager* GetInstance();

	// コンストラクタ
	// デストラクタ
	CollisionManager() = default;
	~CollisionManager() = default;

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// ワールドトランスフォームの初期化
	/// </summary>
	void UpdateWorldTransform();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	/// <summary>
	/// ImGui
	/// </summary>
	void DebugImGui();

public:
	/// <summary>
	/// リセット
	/// </summary>
	void Reset();

	/// <summary>
	/// コライダー2つの衝突判定と応答
	/// </summary>
	void CheckCollisionPair(Collider* colliderA, Collider* colliderB);

	/// <summary>
	/// 全ての当たり判定チェック
	/// </summary>
	void CheckAllCollisions();

	/// <summary>
	/// リストに登録
	/// </summary>
	void AddCollider(Collider* collider);


	void ApplyGlobalVariables();

	bool TestAxis(const Vector3& axis, const Vector3& T, const Vector3 axesA[3], const Vector3& halfExtentsA, const Vector3 axesB[3], const Vector3& halfExtentsB);

private:

	// コピーコンストラクタと代入演算子を削除して複製を防ぐ
	CollisionManager(const CollisionManager&) = delete;
	CollisionManager& operator=(const CollisionManager&) = delete;

	// コライダー
	std::list<Collider*> colliders_;
	// bool型
	bool isDrawCollider_ = false;
	std::unique_ptr<Object3d> obj_;
};
