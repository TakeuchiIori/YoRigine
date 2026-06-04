#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

#include "Object3D/Object3d.h"
#include "WorldTransform/WorldTransform.h"
#include <Memory/PoolAllocator.h>
#include "Vector3.h"
#include "Vector4.h"

#include <Collision/AABB/AABBCollider.h>
#include <Collision/Core/BaseCollider.h>
#include <Collision/Core/ColliderPool.h>
#include <Collision/Core/CollisionTypeIdDef.h>

enum class ColliderShapeType : uint32_t {
	kAABB = 0,
	kOBB,
	kSphere,
};

/// <summary>
/// オブジェクトの管理クラス
/// </summary>
class ObjectManager
{
public:
	// ── コライダーテンプレート ────────────────────────────────────────────
	// モデル名をキーに共有される設定。同じモデルを何個置いても設定は1つ。
	// typeId・AABBサイズはここで管理し、有効フラグだけ PlacedObject が個別に持つ。
	struct ColliderTemplate {
		CollisionTypeIdDef typeId = CollisionTypeIdDef::kNone;
		AABB aabbOffset = { {-1.0f,-1.0f,-1.0f}, {1.0f,1.0f,1.0f} };
	};

	// 配置済みオブジェクトの情報
	struct PlacedObject {
		std::unique_ptr<Object3d> object;
		std::unique_ptr<WorldTransform> worldTransform;
		std::string modelName;
		std::string modelPath;

		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector3 rotation = { 0.0f, 0.0f, 0.0f };
		Vector3 scale = { 1.0f, 1.0f, 1.0f };

		// マテリアル色 (rgba)。エディタから編集 / JSON 保存対象
		Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

		int id = 0;
		int parentID = -1;
		bool isActive = true;

		// アニメーション関連
		bool isAnimation = false;
		std::string animationName = "";
		std::shared_ptr<BaseCollider> collider; // コリジョン用コライダー

		// ── コライダー個別設定 ────────────────────────────────────────────
		bool colliderEnabled = false;
		CollisionTypeIdDef colliderTypeId = CollisionTypeIdDef::kNone;
		ColliderShapeType colliderShapeType = ColliderShapeType::kAABB;

		// AABB
		AABB colliderAabbOffset = { {-1.0f,-1.0f,-1.0f}, {1.0f,1.0f,1.0f} };

		// OBB
		Vector3 colliderObbCenter = {};
		Vector3 colliderObbSize = { 1.0f, 1.0f, 1.0f };
		Vector3 colliderObbEuler = {};

		// Sphere
		Vector3 colliderSphereCenter = {};
		float colliderSphereRadius = 1.0f;

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

	Object3d* GetObject3dById(int id);
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

	///************************* アクセッサ *************************///

	int GetObjectCount() const { return static_cast<int>(idToObject_.size()); }
	int GetNextObjectId() const { return nextObjectId_; }
	void SetCamera(Camera* camera) { camera_ = camera; }

	// 直前フレームの Frustum culling 統計 (シーンエディタで確認用)
	int GetLastFrameCulledCount() const { return lastFrameCulledCount_; }
	int GetLastFrameTotalCount() const { return lastFrameTotalCount_; }

	///************************* コライダー操作 *************************///

	// オブジェクト固有の設定をAABBコライダーに反映する
	void ApplyColliderTemplate(PlacedObject& obj);

	// 指定オブジェクトの colliderTypeId/colliderAabbOffset を同名オブジェクト全員にコピーして反映する（明示的な一括適用用）
	void CopyColliderSettingsToAll(const PlacedObject& src);

	// 同名モデルの colliderEnabled を一括設定する
	void SetColliderEnabledAll(const std::string& modelName, bool enabled);

	///************************* マテリアル色操作 *************************///

	// PlacedObject の color を内部 Object3d のマテリアルに反映する
	void ApplyObjectColor(PlacedObject& obj);

	///************************* コライダー自動フィット *************************///

	// モデルの全頂点からローカル AABB を計算（描画と同じ root ノード行列を反映済み）。
	// 頂点が無い／モデル未ロード時は false。
	bool ComputeModelLocalAABB(const PlacedObject& obj, AABB& outAabb) const;

	// 計算した AABB を margin (1.0 で等倍, 1.05 で 5% 拡大) で膨らませて、
	// 現在の colliderShapeType に応じて AABB/OBB/Sphere の各オフセットに書き込み、適用する。
	bool FitColliderToModel(PlacedObject& obj, float margin);

private:
	ObjectManager() = default;
	~ObjectManager() = default;
	ObjectManager(const ObjectManager&) = delete;
	ObjectManager& operator=(const ObjectManager&) = delete;

	Camera* camera_ = nullptr;

	static ObjectManager* instance_;

	// プールアロケータ
	PoolAllocator<PlacedObject, MAX_OBJECTS> objectPool_;

	// IDからオブジェクトへのマッピング
	std::unordered_map<int, PlacedObject*> idToObject_;

	// 次に割り当てるID
	int nextObjectId_ = 0;

	// Frustum culling 統計 (直前フレーム)
	int lastFrameCulledCount_ = 0;
	int lastFrameTotalCount_ = 0;

	// オブジェクトの初期化ヘルパー
	void InitializePlacedObject(PlacedObject& obj, const std::string& modelPath,
		bool isAnimation,
		const std::string& animationName);
};