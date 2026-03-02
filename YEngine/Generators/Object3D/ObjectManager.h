#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

#include "Object3D/Object3d.h"
#include "WorldTransform/WorldTransform.h"
#include <Memory/PoolAllocator.h>
#include "Vector3.h"

/// <summary>
/// オブジェクトの管理クラス
/// </summary>
class ObjectManager
{
public:
	// 配置済みオブジェクトの情報
	struct PlacedObject {
		std::unique_ptr<Object3d> object;
		std::unique_ptr<WorldTransform> worldTransform;
		std::string modelName;
		std::string modelPath;

		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector3 rotation = { 0.0f, 0.0f, 0.0f };
		Vector3 scale = { 1.0f, 1.0f, 1.0f };

		int id = 0;
		int parentID = -1;
		bool isActive = true;

		// アニメーション関連
		bool isAnimation = false;
		std::string animationName = "";

		PlacedObject() = default;
		~PlacedObject() = default;
	};

	// プールのサイズ（必要に応じて調整）
	static constexpr size_t MAX_OBJECTS = 1024;

	///************************* 基本関数 *************************///

	static ObjectManager* GetInstance();

	void Initialize();
	void Update();
	void Finalize();

	///************************* オブジェクト操作 *************************///

	// オブジェクトを作成して配置
	PlacedObject* CreateObject(const std::string& modelPath,
		bool isAnimation = false, const std::string& animationName = "");

	// オブジェクトを削除
	void DeleteObject(int objectId);
	void DeleteObjectByPointer(PlacedObject* obj);

	// すべてのオブジェクトをクリア
	void ClearAllObjects();

	// オブジェクトの複製
	PlacedObject* DuplicateObject(int objectId, const Vector3& positionOffset = { 0,0,0 });

	///************************* オブジェクト検索 *************************///

	PlacedObject* GetObjectById(int id);
	const PlacedObject* GetObjectById(int id) const;

	std::vector<PlacedObject*> GetAllActiveObjects();
	std::vector<const PlacedObject*> GetAllActiveObjects() const;

	// 親子関係の取得
	std::vector<PlacedObject*> GetChildObjects(int parentId);
	PlacedObject* GetParentObject(int objectId);

	///************************* トランスフォーム操作 *************************///

	void UpdateObjectTransform(PlacedObject& obj);
	void UpdateObjectTransform(int objectId);

	// 親子関係の設定
	bool SetParent(int objectId, int parentId);
	void ClearParent(int objectId);

	// 循環参照チェック
	bool HasCircularReference(int objectId, int parentId) const;

	///************************* 階層操作 *************************///

	// オブジェクトとその子を再帰的に収集
	void CollectObjectHierarchy(int rootId, std::vector<PlacedObject*>& collection);

	///************************* ゲッター *************************///

	int GetObjectCount() const { return static_cast<int>(idToObject_.size()); }
	int GetNextObjectId() const { return nextObjectId_; }

private:
	ObjectManager() = default;
	~ObjectManager() = default;
	ObjectManager(const ObjectManager&) = delete;
	ObjectManager& operator=(const ObjectManager&) = delete;

	static ObjectManager* instance_;

	// プールアロケータ
	PoolAllocator<PlacedObject, MAX_OBJECTS> objectPool_;

	// IDからオブジェクトへのマッピング
	std::unordered_map<int, PlacedObject*> idToObject_;

	// 次に割り当てるID
	int nextObjectId_ = 0;

	// オブジェクトの初期化ヘルパー
	void InitializePlacedObject(PlacedObject& obj, const std::string& modelPath,
		bool isAnimation,
		const std::string& animationName);
};