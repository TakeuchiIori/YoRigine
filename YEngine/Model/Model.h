#pragma once

// C++
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <optional>
#include <span>
#include <algorithm>
#include <unordered_map>
#include <list>

// Engine
#include "DirectXCommon.h"
#include "WorldTransform./WorldTransform.h"
#include "Material/Material.h"
#include "Material/MaterialManager.h"
#include "Mesh/Mesh.h"
#include "Node/Node.h"

// Math
#include "MathFunc.h"
#include "Quaternion.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"

// assimp
#include <assimp/scene.h>
#include <map>
#include "Motion/MotionSystem.h"

// モデルクラス
class SrvManager;
class Line;
class ModelCommon;
class Model
{
public:
	///************************* 基本関数 *************************///

	// 初期化
	void Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename, const std::string& animationName = "", bool isMotion = false);

	// アニメーション更新
	void UpdateAnimation();

	// モーション変更
	void SetChangeMotion(const std::string& directoryPath, const std::string& filename, MotionPlayMode playMode, const std::string& animationName = "");

	// 描画
	void Draw();
	// 影描画
	void DrawShadow();
	// ボーン描画
	void DrawBone(Line& line, const Matrix4x4& worldMatrix);

	// モーション制御
	void PlayOnce();
	void PlayLoop();
	void Stop();
	void Resume();

	// デバッグ情報
	void DebugInfo();

private:
	///************************* 読み込み処理 *************************///

	// モデルインデックス読み込み
	void LoadModelIndexFile(const std::string& directoryPath, const std::string& filename);

	// モーションファイル読み込み
	void LoadMotionFile(const std::string& directoryPath, const std::string& filename, const std::string& animationName = "");

	// キャッシュ追加
	static void AddToCache(const std::string& key, const Motion& motion);

	// キャッシュ削除
	static void ClearAnimationCache();

	// ノード読み込み
	void LoadNode(const aiScene* scene);

	// ノード変換適用
	void ApplyNodeTransform(const aiScene* scene, const aiNode* node, const Matrix4x4& parentMatrix);

	// メッシュ読み込み
	void LoadMesh(const aiScene* scene);

	// マテリアル読み込み
	void LoadMaterial(const aiScene* scene, std::string directoryPath);

	// スキンクラスター読み込み
	void LoadSkinCluster(const aiScene* scene);

	// キャッシュサイズ取得
	static size_t GetCacheSize();

	// ボーン有無確認
	static bool HasBones(const aiScene* scene);

public:
	///************************* アクセッサ *************************///

	// ボーン有無取得
	bool GetHasBones() { return hasBones_; }

	// スケルトン取得
	Skeleton* GetSkeleton() { return skeleton_.get(); }

	// スキンクラスター取得
	SkinCluster* GetSkinCluster() { return skinCluster_.get(); }

	// ルートノード取得
	Node GetRootNode() { return *rootNode_; }

	// ジョイント取得
	Joint* GetJointMap(const std::string& name) { return skeleton_->GetJointByName(name); }

	// モーションシステム取得
	MotionSystem* GetMotionSystem() { return motionSystem_.get(); }

	// 名前設定
	void SetName(const std::string& name) { name_ = name; }

	// モデル名取得
	const std::string& GetName() const { return name_; }

	// メッシュ取得
	const std::vector<std::unique_ptr<Mesh>>& GetMeshes() const { return meshes_; }

private:
	///************************* ポインタ *************************///

	ModelCommon* modelCommon_;
	std::vector<std::unique_ptr<Mesh>> meshes_;
	std::vector<std::unique_ptr<Material>> materials_;
	std::unique_ptr<MotionSystem> motionSystem_;
	std::unique_ptr<Skeleton> skeleton_;
	std::unique_ptr<SkinCluster> skinCluster_;
	std::unique_ptr<Node> rootNode_;
	SrvManager* srvManager_ = nullptr;

	///************************* モーション関連 *************************///

	Motion motion_;
	bool isMotion_;
	bool hasBones_;
	float deltaTime_;

public:
	///************************* キャッシュ管理 *************************///
	std::string name_;
	static const std::string binPath;
	static std::unordered_map<std::string, Motion> animationCache_;
	static const size_t MAX_CACHE_SIZE = 50;
	static std::list<std::string> cacheOrder_;
	static std::unordered_map<std::string, std::list<std::string>::iterator> cacheIterators_;
};
