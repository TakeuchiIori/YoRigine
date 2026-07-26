#pragma once

// C++
#include <algorithm>
#include <d3d12.h>
#include <list>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>

// Engine
#include "DirectXCommon.h"
#include "Material/Material.h"
#include "Material/MaterialManager.h"
#include "Mesh/Mesh.h"
#include "Node/Node.h"
#include "WorldTransform./WorldTransform.h"

// Math
#include "MathFunc.h"
#include "Matrix4x4.h"
#include "Quaternion.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

// assimp
#include "Motion/Core/MotionSystem.h"
#include <assimp/scene.h>
#include <map>

// モデルクラス
class ModelCommon;

namespace YoRigine {

class Line;
class SrvManager;

class Model {
public:
  ///************************* 基本関数 *************************///

  // 初期化
  void Initialize(ModelCommon *modelCommon, const std::string &directoryPath,
                  const std::string &filename,
                  const std::string &animationName = "", bool isMotion = false);

  // アニメーション更新
  void UpdateAnimation();

  // モーション変更
  void SetChangeMotion(const std::string &directoryPath,
                       const std::string &filename, MotionPlayMode playMode,
                       const std::string &animationName = "");

  // 上半身専用モーションの再生
  void PlayUpperMotion(const std::string &directoryPath,
                       const std::string &filename, MotionPlayMode playMode,
                       const std::string &animationName = "");

  // 描画。overrideTexturePath
  // を渡すと全メッシュのテクスチャをそれで上書きして描く（空なら本来のテクスチャ）。
  void Draw(const std::string &overrideTexturePath = "");
  // 影描画
  void DrawShadow();
  // ボーン描画
  void DrawBone(YoRigine::Line &line, const Matrix4x4 &worldMatrix);

  // インスタンシング描画 (ObjectInstanced PSO 使用、skinning未対応)
  // instanceSRV: StructuredBuffer<InstanceData> の GPU descriptor handle
  // overrideTexturePath: 空でなければモデル既定テクスチャの代わりにこれを貼る
  // (非インスタンスの Model::Draw と同じアクセントテクスチャ差し替えをバッチ単位で行う)
  void DrawInstanced(uint32_t instanceCount,
                     D3D12_GPU_DESCRIPTOR_HANDLE instanceSRV,
                     const std::string &overrideTexturePath = "");
  // インスタンシング影描画 (ShadowMapInstanced PSO 使用)
  void DrawShadowInstanced(uint32_t instanceCount,
                           D3D12_GPU_DESCRIPTOR_HANDLE instanceSRV);
  // インスタンシング輪郭線描画 (ObjectOutlineInstanced PSO 使用)。
  // PSO/RS/CB は呼び出し側で設定済み。per-instance バッファをバインドして描く。
  void DrawOutlineInstanced(uint32_t instanceCount,
                            D3D12_GPU_DESCRIPTOR_HANDLE instanceSRV);

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
  void LoadModelIndexFile(const std::string &directoryPath,
                          const std::string &filename);

  // モーションファイル読み込み
  void LoadMotionFile(const std::string &directoryPath,
                      const std::string &filename,
                      const std::string &animationName = "");

  // キャッシュ追加
  static void AddToCache(const std::string &key, const Motion &motion);

  // キャッシュ削除
  static void ClearAnimationCache();

  // ノード読み込み
  void LoadNode(const aiScene *scene);

  // ノード変換適用
  void ApplyNodeTransform(const aiScene *scene, const aiNode *node,
                          const Matrix4x4 &parentMatrix);

  // メッシュ読み込み
  void LoadMesh(const aiScene *scene);

  // マテリアル読み込み
  void LoadMaterial(const aiScene *scene, std::string directoryPath);

  // スキンクラスター読み込み
  void LoadSkinCluster(const aiScene *scene);

  // キャッシュサイズ取得
  static size_t GetCacheSize();

  // ボーン有無確認
  static bool HasBones(const aiScene *scene);

public:
  ///************************* アクセッサ *************************///

  // ボーン有無取得
  bool GetHasBones() { return hasBones_; }

  // スケルトン取得
  Skeleton *GetSkeleton() { return skeleton_.get(); }

  // スキンクラスター取得
  SkinCluster *GetSkinCluster() { return skinCluster_.get(); }

  // ルートノード取得
  Node GetRootNode() { return *rootNode_; }

  // ジョイント取得
  Joint *GetJointMap(const std::string &name) {
    return skeleton_->GetJointByName(name);
  }

  // モーションシステム取得
  MotionSystem *GetMotionSystem() { return motionSystem_.get(); }

  // 名前設定
  void SetName(const std::string &name) { name_ = name; }

  // モデル名取得
  const std::string &GetName() const { return name_; }

  // メッシュ取得
  const std::vector<std::unique_ptr<Mesh>> &GetMeshes() const {
    return meshes_;
  }

  // マテリアルの拡散反射色(Kd)から「白でも黒でもない」最初の色を返す。
  // マルチメッシュ/マルチマテリアルなら白以外の色が採用される。
  // 見つかれば out に格納して true、無ければ false。
  bool TryGetThemeColor(Vector4 &out) const;

  // マテリアルの中から「実テクスチャ（＝白デフォルト以外）を持つ最初の1つ」のテクスチャパスを返す。
  // マルチメッシュで一方にだけ画像を割り当てた場合の“アクセント画像”を取り出す用途。
  // 見つかれば out に格納して true、無ければ false。
  bool TryGetAccentTexturePath(std::string &out) const;

  // Mixamo由来モデルか
  bool IsMixamoAsset() const { return isMixamoAsset_; }

private:
  ///************************* ポインタ *************************///

  ModelCommon *modelCommon_;
  std::vector<std::unique_ptr<Mesh>> meshes_;
  std::vector<std::unique_ptr<Material>> materials_;
  std::unique_ptr<MotionSystem> motionSystem_;
  std::unique_ptr<Skeleton> skeleton_;
  std::unique_ptr<SkinCluster> skinCluster_;
  std::unique_ptr<Node> rootNode_;
  YoRigine::SrvManager *srvManager_ = nullptr;

  ///************************* モーション関連 *************************///

  Motion motion_;
  // 再生中モーションはModelインスタンスごとに管理する。
  // staticにすると他オブジェクトの切替がPlayerの切替判定へ干渉する。
  std::string currentMotionCacheKey_;
  bool isMotion_;
  bool hasBones_;
  bool isMixamoAsset_ = false;
  float importUnitScale_ = 1.0f;
  float deltaTime_;

public:
  ///************************* キャッシュ管理 *************************///
  std::string name_;
  static const std::string binPath;
  static std::unordered_map<std::string, Motion> animationCache_;
  static const size_t MAX_CACHE_SIZE = 50;
  static std::list<std::string> cacheOrder_;
  static std::unordered_map<std::string, std::list<std::string>::iterator>
      cacheIterators_;
};

} // namespace YoRigine
