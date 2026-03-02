#pragma once
// Engine
#include "Systems/Camera/Camera.h"
#include "WorldTransform/WorldTransform.h"
#include "Object3D/Object3d.h"
#include "Loaders/Json/JsonManager.h"

// Collision
#include "Collision/Core/BaseCollider.h"
#include "Collision/Core/ColliderFactory.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include "Collision/OBB/OBBCollider.h"
#include "Collision/AABB/AABBCollider.h"
#include "Collision/Sphere/SphereCollider.h"

/// <summary>
/// オブジェクトの基底クラス
/// </summary>
class BaseObject {
public:

	///************************* 基本的な関数 *************************///
	virtual ~BaseObject() = default;
	virtual void Initialize(Camera* camera) = 0;
	virtual void InitCollision() = 0;
	virtual void InitJson() = 0;

	virtual void Update() = 0;

	virtual void Draw() = 0;
	virtual void DrawAnimation() {}
	virtual void DrawCollision() {}


	///************************* 当たり判定 *************************///
	virtual void OnEnterCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {}
	virtual void OnCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {}
	virtual void OnExitCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {}
	virtual void OnDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir) {}
	virtual void OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir) {}



	///************************* SRT *************************///
	const Vector3& GetTranslate() const { return wt_.translate_; }
	void SetTranslate(const Vector3& pos) { wt_.translate_ = pos; }
	const Vector3& GetRotae() const { return wt_.rotate_; }
	void SetRotae(const Vector3& rot) { wt_.rotate_ = rot; }
	const Vector3& GetScale() const { return wt_.scale_; }
	void SetScale(const Vector3& scale) { wt_.scale_ = scale; }

	///************************* その他 *************************///

	const WorldTransform& GetWT() const { return wt_; }
	WorldTransform& GetWT() { return wt_; }


protected:
	WorldTransform wt_;
	Camera* camera_ = nullptr;
	std::unique_ptr<Object3d> obj_;
	std::unique_ptr<YoRigine::JsonManager> jsonManager_;
	std::unique_ptr<YoRigine::JsonManager> jsonCollider_;
	std::shared_ptr<OBBCollider> obbCollider_;
	std::shared_ptr<AABBCollider> aabbCollider_;
	std::shared_ptr<SphereCollider> sphereCollider_;
};
