#include "ObjectManager.h"
#include "ModelManager.h"
#include <iostream>
#include <algorithm>

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
	for (auto& [id, obj] : idToObject_) {
		if (obj && obj->isActive && obj->object) {
			obj->object->UpdateAnimation();
		}
	}
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
	const std::string& animationName)
{
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
	const Vector3& positionOffset)
{
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

	UpdateObjectTransform(*duplicate);

	std::cout << "複製: 元ID=" << objectId << " 新ID=" << duplicate->id << std::endl;
	return duplicate;
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
	std::vector<PlacedObject*>& collection)
{
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
	const std::string& animationName)
{
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

	UpdateObjectTransform(obj);
}
