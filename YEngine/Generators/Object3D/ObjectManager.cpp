#include "ObjectManager.h"
#include "ModelManager.h"
#include <iostream>
#include <algorithm>

#include <Collision/Core/CollisionManager.h>
#include <Collision/Core/ColliderFactory.h>
#include <Collision/OBB/OBBCollider.h>
#include <Collision/Sphere/SphereCollider.h>
ObjectManager* ObjectManager::instance_ = nullptr;


/// <summary>
/// シングルトンインスタンス取得
/// </summary>
ObjectManager* ObjectManager::GetInstance() {
	if (!instance_) {
		instance_ = new ObjectManager;
	}
	return instance_;
}


/// <summary>
/// マネージャ初期化（プール・ID管理のリセット）
/// </summary>
void ObjectManager::Initialize() {
	objectPool_.Clear();
	idToObject_.clear();
	nextObjectId_ = 0;
}


/// <summary>
/// アクティブオブジェクトのアニメーション更新
/// </summary>
void ObjectManager::Update() {
	auto* cm = YoRigine::CollisionManager::GetInstance();
	const bool cullingActive = cm->IsCullingActive();

	int total = 0;
	int culled = 0;

	for (auto& [id, obj] : idToObject_) {
		if (!obj) continue;
		++total;

		// Frustum culling: コライダー個別フラグ checkOutsideCamera=true で
		// 前フレームの世界 AABB が視錐台外なら、アニメーションも collider Update も丸ごとスキップ。
		// 静的オブジェクトでは AABB が前フレームと同じなので劣化なし。
		// 動かしたいものは BaseCollider::SetCheckOutsideCamera(false) でオプトアウトする。
		if (cullingActive && obj->collider && obj->collider->IsCheckOutsideCamera()) {
			AABB aabb = YoRigine::CollisionManager::ComputeWorldAABB(obj->collider.get());
			if (cm->IsAABBOutsideCullingFrustum(aabb)) {
				++culled;
				continue;
			}
		}

		if (obj->isActive && obj->object) {
			obj->object->UpdateAnimation();
		}
		// colliderEnabled に関わらず Update() してワールドAABBを常に最新に保つ。
		// 衝突判定への参加は collider->IsCollisionEnabled() で制御される。
		if (obj->collider) {
			obj->collider->Update();
		}
	}

	lastFrameTotalCount_ = total;
	lastFrameCulledCount_ = culled;
}


/// <summary>
/// 全オブジェクト削除して終了
/// </summary>
void ObjectManager::Finalize() {
	ClearAllObjects();
	delete instance_;
	instance_ = nullptr;
}


/// <summary>
/// 新しいオブジェクトの生成
/// </summary>
ObjectManager::PlacedObject* ObjectManager::CreateObject(
	const std::string& modelPath,
	bool isAnimation,
	const std::string& animationName) {
	PlacedObject* newObj = objectPool_.Alloc();
	if (!newObj) {
		std::cout << "オブジェクトプールが満杯です。" << std::endl;
		return nullptr;
	}

	newObj->id = nextObjectId_++;
	InitializePlacedObject(*newObj, modelPath, isAnimation, animationName);

	idToObject_[newObj->id] = newObj;

	std::cout << "オブジェクト生成: ID=" << newObj->id
		<< " モデル=" << modelPath
		<< (isAnimation ? "（アニメーション）" : "") << std::endl;

	return newObj;
}


/// <summary>
/// オブジェクト削除
/// </summary>
void ObjectManager::DeleteObject(int objectId) {

	auto it = idToObject_.find(objectId);
	if (it == idToObject_.end()) return;

	PlacedObject* obj = it->second;

	// 子の親をクリア
	for (auto& [id, child] : idToObject_) {
		if (child && child->parentID == objectId) {
			child->parentID = -1;
			UpdateObjectTransform(*child);
		}
	}

	idToObject_.erase(it);
	objectPool_.Free(obj);

	std::cout << "オブジェクト削除: ID=" << objectId << std::endl;
}


/// <summary>
/// ポインタ版削除
/// </summary>
void ObjectManager::DeleteObjectByPointer(PlacedObject* obj) {
	if (obj) DeleteObject(obj->id);
}


/// <summary>
/// 全オブジェクト削除
/// </summary>
void ObjectManager::ClearAllObjects() {
	idToObject_.clear();
	objectPool_.Clear();
	nextObjectId_ = 0;

	std::cout << "すべてのオブジェクトを削除しました。" << std::endl;
}


/// <summary>
/// オブジェクトの複製
/// </summary>
ObjectManager::PlacedObject* ObjectManager::DuplicateObject(
	int objectId,
	const Vector3& positionOffset) {
	PlacedObject* original = GetObjectById(objectId);
	if (!original) return nullptr;

	PlacedObject* duplicate = CreateObject(
		original->modelPath,
		original->isAnimation,
		original->animationName
	);
	if (!duplicate) return nullptr;

	// トランスフォーム複製
	duplicate->position = original->position + positionOffset;
	duplicate->rotation = original->rotation;
	duplicate->scale = original->scale;
	duplicate->parentID = original->parentID;

	// コライダー設定を複製（各オブジェクト固有の設定をそのままコピー）
	duplicate->colliderEnabled      = original->colliderEnabled;
	duplicate->colliderTypeId       = original->colliderTypeId;
	duplicate->colliderShapeType    = original->colliderShapeType;
	duplicate->colliderAabbOffset   = original->colliderAabbOffset;
	duplicate->colliderObbCenter    = original->colliderObbCenter;
	duplicate->colliderObbSize      = original->colliderObbSize;
	duplicate->colliderObbEuler     = original->colliderObbEuler;
	duplicate->colliderSphereCenter = original->colliderSphereCenter;
	duplicate->colliderSphereRadius = original->colliderSphereRadius;
	ApplyColliderTemplate(*duplicate);

	// マテリアル色を複製
	duplicate->color = original->color;
	ApplyObjectColor(*duplicate);

	// UV スケール・ステキャスティック強度を複製
	duplicate->uvScale     = original->uvScale;
	duplicate->uvStochastic = original->uvStochastic;
	ApplyObjectUV(*duplicate);

	UpdateObjectTransform(*duplicate);

	std::cout << "複製: 元ID=" << objectId << " 新ID=" << duplicate->id << std::endl;
	return duplicate;
}


Object3d* ObjectManager::GetObject3dById(int id) {
	PlacedObject* obj = GetObjectById(id);
	return (obj && obj->object) ? obj->object.get() : nullptr;
}

/// <summary>
/// ID から取得
/// </summary>
ObjectManager::PlacedObject* ObjectManager::GetObjectById(int id) {
	auto it = idToObject_.find(id);
	return (it != idToObject_.end()) ? it->second : nullptr;
}

const ObjectManager::PlacedObject* ObjectManager::GetObjectById(int id) const {
	auto it = idToObject_.find(id);
	return (it != idToObject_.end()) ? it->second : nullptr;
}


/// <summary>
/// アクティブなオブジェクト一覧を取得
/// </summary>
std::vector<ObjectManager::PlacedObject*> ObjectManager::GetAllActiveObjects() {
	std::vector<PlacedObject*> result;
	result.reserve(idToObject_.size());

	for (auto& [id, obj] : idToObject_) {
		if (obj && obj->isActive) result.push_back(obj);
	}
	return result;
}

std::vector<const ObjectManager::PlacedObject*> ObjectManager::GetAllActiveObjects() const {
	std::vector<const PlacedObject*> result;
	result.reserve(idToObject_.size());

	for (const auto& [id, obj] : idToObject_) {
		if (obj && obj->isActive) result.push_back(obj);
	}
	return result;
}


/// <summary>
/// 子オブジェクト一覧を取得
/// </summary>
std::vector<ObjectManager::PlacedObject*> ObjectManager::GetChildObjects(int parentId) {

	std::vector<PlacedObject*> children;
	for (auto& [id, obj] : idToObject_) {
		if (obj && obj->parentID == parentId) {
			children.push_back(obj);
		}
	}
	return children;
}


/// <summary>
/// 親オブジェクト取得
/// </summary>
ObjectManager::PlacedObject* ObjectManager::GetParentObject(int objectId) {
	PlacedObject* obj = GetObjectById(objectId);
	if (!obj || obj->parentID == -1) return nullptr;
	return GetObjectById(obj->parentID);
}


/// <summary>
/// トランスフォーム更新（親子階層にも対応）
/// </summary>
void ObjectManager::UpdateObjectTransform(PlacedObject& obj) {

	if (!obj.worldTransform) return;

	// 循環参照チェック
	if (obj.parentID >= 0 && HasCircularReference(obj.id, obj.parentID)) {
		obj.parentID = -1;
		std::cout << "循環参照検出: ID=" << obj.id << " の親を解除" << std::endl;
	}

	// 親の適用
	if (obj.parentID >= 0) {
		PlacedObject* parent = GetObjectById(obj.parentID);
		obj.worldTransform->parent_ =
			(parent && parent->worldTransform) ? parent->worldTransform.get() : nullptr;
		if (!parent) obj.parentID = -1;
	} else {
		obj.worldTransform->parent_ = nullptr;
	}

	// ローカル値を反映
	obj.worldTransform->translate_ = obj.position;
	obj.worldTransform->rotate_ = obj.rotation;
	obj.worldTransform->scale_ = obj.scale;

	obj.worldTransform->UpdateMatrix();

	// 子も更新
	for (auto* child : GetChildObjects(obj.id)) {
		UpdateObjectTransform(*child);
	}
}


/// <summary>
/// ID 指定でトランスフォーム更新
/// </summary>
void ObjectManager::UpdateObjectTransform(int objectId) {
	if (auto* obj = GetObjectById(objectId)) {
		UpdateObjectTransform(*obj);
	}
}


/// <summary>
/// 親設定（循環参照チェックつき）
/// </summary>
bool ObjectManager::SetParent(int objectId, int parentId) {

	PlacedObject* obj = GetObjectById(objectId);
	if (!obj) return false;

	// 循環防止
	if (parentId >= 0 && HasCircularReference(objectId, parentId)) {
		std::cout << "親設定失敗：循環参照" << std::endl;
		return false;
	}

	// 存在確認
	if (parentId >= 0 && !GetObjectById(parentId)) {
		std::cout << "親設定失敗：親が存在しません" << std::endl;
		return false;
	}

	obj->parentID = parentId;
	UpdateObjectTransform(*obj);
	return true;
}


/// <summary>
/// 親クリア
/// </summary>
void ObjectManager::ClearParent(int objectId) {
	if (auto* obj = GetObjectById(objectId)) {
		obj->parentID = -1;
		UpdateObjectTransform(*obj);
	}
}


/// <summary>
/// 循環参照チェック
/// </summary>
bool ObjectManager::HasCircularReference(int objectId, int parentId) const {
	if (parentId == -1) return false;
	if (objectId == parentId) return true;

	const PlacedObject* parent = GetObjectById(parentId);
	if (!parent) return false;

	return HasCircularReference(objectId, parent->parentID);
}


/// <summary>
/// 親子階層を再帰的に収集
/// </summary>
void ObjectManager::CollectObjectHierarchy(
	int rootId,
	std::vector<PlacedObject*>& collection) {
	PlacedObject* root = GetObjectById(rootId);
	if (!root) return;

	collection.push_back(root);

	for (auto* child : GetChildObjects(rootId)) {
		CollectObjectHierarchy(child->id, collection);
	}
}


/// <summary>
/// PlacedObject 初期化
/// </summary>
void ObjectManager::InitializePlacedObject(
	PlacedObject& obj,
	const std::string& modelPath,
	bool isAnimation,
	const std::string& animationName) {
	// モデル情報
	obj.modelPath = modelPath;

	// ファイル名抽出
	std::filesystem::path path(modelPath);
	obj.modelName = path.filename().string();

	obj.isAnimation = isAnimation;
	obj.animationName = animationName;

	// Object3d 生成
	obj.object = std::make_unique<Object3d>();
	obj.object->Initialize();
	obj.object->SetModel(obj.modelName, isAnimation, animationName);

	// WorldTransform 初期化
	obj.worldTransform = std::make_unique<WorldTransform>();
	obj.worldTransform->Initialize();

	// 基本トランスフォーム設定
	obj.position = { 0.0f, 0.0f, 0.0f };
	obj.rotation = { 0.0f, 0.0f, 0.0f };
	obj.scale = { 1.0f, 1.0f, 1.0f };
	obj.parentID = -1;
	obj.isActive = true;

	// 当たり判定（初期状態では無効）
	obj.collider = nullptr;

	// マテリアル色（プール再利用時の残骸を防ぐため明示的にリセット）
	obj.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	ApplyObjectColor(obj);

	// UV スケール・ステキャスティック強度も同様にリセット
	obj.uvScale = { 1.0f, 1.0f };
	obj.uvStochastic = 0.0f;
	ApplyObjectUV(obj);

	UpdateObjectTransform(obj);
}

void ObjectManager::ApplyObjectColor(PlacedObject& obj) {
	if (!obj.object) return;
	obj.object->SetMaterialColor(obj.color);
}

void ObjectManager::ApplyObjectUV(PlacedObject& obj) {
	if (!obj.object) return;
	// Object3d::uvScale は public メンバ。Draw() 内の UpdateUV() が拾って CB に書き込む。
	obj.object->uvScale = obj.uvScale;
	obj.object->SetStochasticStrength(obj.uvStochastic);
}

bool ObjectManager::ComputeModelLocalAABB(const PlacedObject& obj, AABB& outAabb) const {
	if (!obj.object) return false;
	Model* model = obj.object->GetModel();
	if (!model) return false;
	const auto& meshes = model->GetMeshes();
	if (meshes.empty()) return false;

	// 描画パス（ボーンなし）では Root ノードのローカル行列が適用されるため、ここでも反映する
	const Matrix4x4 rootMtx = model->GetHasBones()
		? MakeIdentity4x4()
		: model->GetRootNode().GetLocalMatrix();

	bool any = false;
	Vector3 mn = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
	Vector3 mx = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	for (const auto& m : meshes) {
		if (!m) continue;
		const auto& md = m->GetMeshData();
		for (const auto& v : md.vertices) {
			Vector3 p = Transform({ v.position.x, v.position.y, v.position.z }, rootMtx);
			mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mn.z = std::min(mn.z, p.z);
			mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); mx.z = std::max(mx.z, p.z);
			any = true;
		}
	}

	if (!any) return false;
	outAabb.min = mn;
	outAabb.max = mx;
	return true;
}

bool ObjectManager::FitColliderToModel(PlacedObject& obj, float margin) {
	AABB local{};
	if (!ComputeModelLocalAABB(obj, local)) return false;

	// margin で中心まわりに均等拡大
	const Vector3 center = (local.min + local.max) * 0.5f;
	const Vector3 halfExtent = {
		(local.max.x - local.min.x) * 0.5f * margin,
		(local.max.y - local.min.y) * 0.5f * margin,
		(local.max.z - local.min.z) * 0.5f * margin,
	};

	switch (obj.colliderShapeType) {
	case ColliderShapeType::kAABB:
		obj.colliderAabbOffset.min = { center.x - halfExtent.x, center.y - halfExtent.y, center.z - halfExtent.z };
		obj.colliderAabbOffset.max = { center.x + halfExtent.x, center.y + halfExtent.y, center.z + halfExtent.z };
		break;
	case ColliderShapeType::kOBB:
		// OBB.size は半サイズ (YMath/Shape/OBB.h コメント参照) なので halfExtent をそのまま入れる
		obj.colliderObbCenter = center;
		obj.colliderObbSize   = halfExtent;
		obj.colliderObbEuler  = { 0.0f, 0.0f, 0.0f };
		break;
	case ColliderShapeType::kSphere: {
		obj.colliderSphereCenter = center;
		const float r = std::sqrt(halfExtent.x * halfExtent.x +
		                          halfExtent.y * halfExtent.y +
		                          halfExtent.z * halfExtent.z);
		obj.colliderSphereRadius = r;
		break;
	}
	}

	ApplyColliderTemplate(obj);
	return true;
}

void ObjectManager::ApplyColliderTemplate(PlacedObject& obj) {
	// シェイプが変わった場合はコライダーを作り直す
	bool needRebuild = !obj.collider;
	if (!needRebuild) {
		switch (obj.colliderShapeType) {
		case ColliderShapeType::kAABB:   needRebuild = !dynamic_cast<AABBCollider*>(obj.collider.get());   break;
		case ColliderShapeType::kOBB:    needRebuild = !dynamic_cast<OBBCollider*>(obj.collider.get());    break;
		case ColliderShapeType::kSphere: needRebuild = !dynamic_cast<SphereCollider*>(obj.collider.get()); break;
		}
	}

	if (needRebuild) {
		obj.collider = nullptr;
		switch (obj.colliderShapeType) {
		case ColliderShapeType::kAABB:
			obj.collider = ColliderFactory::CreateStatic<AABBCollider>(
				obj.worldTransform.get(), static_cast<uint32_t>(CollisionTypeIdDef::kNone));
			break;
		case ColliderShapeType::kOBB:
			obj.collider = ColliderFactory::CreateStatic<OBBCollider>(
				obj.worldTransform.get(), static_cast<uint32_t>(CollisionTypeIdDef::kNone));
			break;
		case ColliderShapeType::kSphere:
			obj.collider = ColliderFactory::CreateStatic<SphereCollider>(
				obj.worldTransform.get(), static_cast<uint32_t>(CollisionTypeIdDef::kNone));
			break;
		}
	}

	if (!obj.collider) return;

	obj.collider->SetTypeID(static_cast<uint32_t>(obj.colliderTypeId));
	obj.collider->SetEnablePenetration(true);
	obj.collider->SetIsStatic(true);
	obj.collider->SetCollisionEnabled(obj.colliderEnabled);

	// シェイプ別のオフセットを反映
	switch (obj.colliderShapeType) {
	case ColliderShapeType::kAABB:
		if (auto* c = dynamic_cast<AABBCollider*>(obj.collider.get()))
			c->aabbOffset_ = obj.colliderAabbOffset;
		break;
	case ColliderShapeType::kOBB:
		if (auto* c = dynamic_cast<OBBCollider*>(obj.collider.get())) {
			c->obbOffset_.center = obj.colliderObbCenter;
			c->obbOffset_.size   = obj.colliderObbSize;
			c->obbEulerOffset_   = obj.colliderObbEuler;
		}
		break;
	case ColliderShapeType::kSphere:
		if (auto* c = dynamic_cast<SphereCollider*>(obj.collider.get())) {
			c->sphereOffset_.center = obj.colliderSphereCenter;
			c->SetRadius(obj.colliderSphereRadius);
		}
		break;
	}

	obj.collider->Update();
}

void ObjectManager::CopyColliderSettingsToAll(const PlacedObject& src) {
	// ソースオブジェクトの個別設定を同名オブジェクト全員にコピーして反映する。
	// colliderEnabled も含める: 含めないとコピー先が無効のまま (デバッグ描画も判定もオフ)
	// 残り、「コピーしたのに同じに見えない」原因になる。
	for (auto& [id, obj] : idToObject_) {
		if (obj && obj->modelName == src.modelName && obj->id != src.id) {
			obj->colliderEnabled      = src.colliderEnabled;
			obj->colliderTypeId       = src.colliderTypeId;
			obj->colliderShapeType    = src.colliderShapeType;
			obj->colliderAabbOffset   = src.colliderAabbOffset;
			obj->colliderObbCenter    = src.colliderObbCenter;
			obj->colliderObbSize      = src.colliderObbSize;
			obj->colliderObbEuler     = src.colliderObbEuler;
			obj->colliderSphereCenter = src.colliderSphereCenter;
			obj->colliderSphereRadius = src.colliderSphereRadius;
			ApplyColliderTemplate(*obj);
		}
	}
}

void ObjectManager::SetColliderEnabledAll(const std::string& modelName, bool enabled) {
	for (auto& [id, obj] : idToObject_) {
		if (obj && obj->modelName == modelName) {
			obj->colliderEnabled = enabled;
			ApplyColliderTemplate(*obj);
		}
	}
}