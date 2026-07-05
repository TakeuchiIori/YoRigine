#pragma once
// Engine
#include "Systems/Camera/Camera.h"
#include "WorldTransform/WorldTransform.h"
#include "Object3D/Object3d.h"
#include "Loaders/Json/JsonManager.h"

// C++
#include <string>

// Collision
#include "Collision/Core/BaseCollider.h"
#include "Collision/Core/ColliderFactory.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include "Collision/OBB/OBBCollider.h"
#include "Collision/AABB/AABBCollider.h"
#include "Collision/Sphere/SphereCollider.h"
#include "Collision/Capsule/CapsuleCollider.h"

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
	// 影パス描画。BaseObjectManager から一括で呼べるよう基底に持たせる。
	// 影を落とさないオブジェクトは未オーバーライドのまま (デフォルト空) で良い。
	virtual void DrawShadow() {}


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

	// 内部 Object3d への参照（マテリアル/ディゾルブ等を State 側から操作するため）
	Object3d* GetObject3d() const { return obj_.get(); }

	// インスタンシング描画の対象にできるか。
	// 既定: モデルを持ち、ボーン（＝スケルタルアニメ）を持たない非アニメオブジェクト。
	// ディゾルブや独自 Draw を行う等、個別描画が必要なオブジェクトは false を返すよう
	// override して従来の Draw() 経路に残すこと。
	virtual bool IsInstanceable() const {
		if (!obj_) return false;
		Model* model = obj_->GetModel();
		return model && !model->GetHasBones();
	}


	///************************* マネージャ連携 *************************///

	// シーン内での表示名。BaseObjectManager のインスペクタ表示 / 名前検索に使う。
	const std::string& GetName() const { return name_; }
	void SetName(const std::string& name) { name_ = name; }

	// アクティブフラグ。false の間は BaseObjectManager の一括 Update/Draw でスキップされる。
	bool IsActive() const { return active_; }
	void SetActive(bool active) { active_ = active; }

	// インスペクタの詳細欄に独自項目を出したいオブジェクトはこれをオーバーライドする。
	// デフォルトは空 (名前 / アクティブ / SRT は BaseObjectManager 側が共通描画する)。
	virtual void DrawInspector() {}


protected:
	WorldTransform wt_;
	Camera* camera_ = nullptr;
	std::unique_ptr<Object3d> obj_;
	std::unique_ptr<YoRigine::JsonManager> jsonManager_;
	std::unique_ptr<YoRigine::JsonManager> jsonCollider_;
	std::shared_ptr<OBBCollider> obbCollider_;
	std::shared_ptr<AABBCollider> aabbCollider_;
	std::shared_ptr<SphereCollider> sphereCollider_;
	std::shared_ptr<CapsuleCollider> capsuleCollider_;

	std::string name_;       // 表示名 / 検索キー (BaseObjectManager 用)
	bool active_ = true;     // false なら一括更新・描画の対象外
};
