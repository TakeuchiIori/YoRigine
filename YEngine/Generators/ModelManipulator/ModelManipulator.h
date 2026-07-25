#pragma once

// C++
#include <string>
#include <vector>

// Engine
#include <Motion/Editor/MotionEditor.h>
#include <Object3D/ObjectManager.h>
#include <Systems/Camera/Camera.h>

#ifdef USE_IMGUI
#include "ModelBrowser.h"
#include "PlacedObjectGizmable.h"
#include "SceneEditorUI.h"
#include "StampMode.h"
#include <Debugger/Gizmo/GizmoController.h>
#endif

// Subsystems
#include "ObjectSelector.h"
#include "PickBuffer.h"
#include "PrefabManager.h"
#include "SceneSerializer.h"
#include <Graphics/Drawer/InstancedShape/InstancedCube.h>
#include <Graphics/Drawer/InstancedShape/InstancedSphere.h>
#include <Graphics/Drawer/LineManager/Line.h>

namespace YoRigine {

class ModelManipulator {
public:
  //=========================================================================
  // 基本
  //=========================================================================
  static ModelManipulator *GetInstance();

  void Initialize();
  void Update();
  void Draw();
  void DrawLine();
  void DrawPickPass();
  void DrawShadow();
  void DrawImGui();
  void DrawGizmo();
  void DrawForPick();
  void Finalize();

  void PlaceObject(const std::string &modelPath);
  void LoadScene(const std::string &sceneName);

  void SetCamera(YoRigine::Camera *camera) {
    camera_ = camera;
    selector_.SetCamera(camera);
    motionEditor_.SetCamera(camera);
    // AABB/OBB は InstancedCube 集約描画
    colliderCubes_.SetCamera(camera_);
    // Sphere は InstancedSphere 集約描画
    colliderSpheres_.SetCamera(camera_);
    // Capsule は Line で描画
    colliderLineCapsule_.SetCamera(camera_);
    objectManager_->SetCamera(camera);
#ifdef USE_IMGUI
    stampMode_.SetCamera(camera);
#endif
  }

  //=========================================================================
  // サブシステムアクセッサ
  //=========================================================================
  SceneSerializer &GetSerializer() { return serializer_; }
  ObjectSelector &GetSelector() { return selector_; }
  MotionEditor &GetMotionEditor() { return motionEditor_; }

#ifdef USE_IMGUI
  // シーンエディタ (オブジェクト選択・ギズモ) が有効かどうか。
  // オブジェクト一覧ウィンドウが閉じている、または Editor 全体が非表示なら
  // false。 この返り値で ObjectSelector::Update / DrawGizmo を抑制する。
  bool IsSceneEditorActive() const;

  //=========================================================================
  // デバッグ描画設定（SceneEditorUI から操作）
  //=========================================================================
  bool *GetShowColliderDebugPtr() { return &showColliderDebug_; }
  bool *GetShowColliderSelectedOnlyPtr() { return &showColliderSelectedOnly_; }
  bool *GetShowBroadPhaseGridPtr() { return &showBroadPhaseGrid_; }
  float *GetBroadPhaseGridDrawRadiusPtr() { return &broadPhaseGridDrawRadius_; }
  bool *GetEnableDrawFrustumCullingPtr() { return &enableDrawFrustumCulling_; }
#endif

private:
  //=========================================================================
  // 内部処理
  //=========================================================================
  void ShortcutKey();
  void CopyObject();
  void PasteObject();

  //=========================================================================
  // シングルトン
  //=========================================================================
  ModelManipulator() = default;
  ~ModelManipulator() = default;
  ModelManipulator(const ModelManipulator &) = delete;
  ModelManipulator &operator=(const ModelManipulator &) = delete;
  ModelManipulator(ModelManipulator &&) = delete;
  ModelManipulator &operator=(ModelManipulator &&) = delete;

  //=========================================================================
  // メンバ変数
  //=========================================================================
  YoRigine::Camera *camera_ = nullptr;
  ObjectManager *objectManager_ = nullptr;
  PickBuffer *pickBuffer_ = nullptr;
  bool isInitialized_ = false;
  std::string jsonPath_;
  std::string modelFolderPath_ = "Resources/Models/";
  // 現在ロード中のシーン名 (LoadScene 切替で前シーンを ObjectManager
  // に退避するキーに使う)
  std::string currentSceneName_;
  MotionEditor motionEditor_;

  // AABB/OBB はインスタンス描画(1 DrawInstanced)。色はインスタンス毎。
  InstancedCube colliderCubes_;
  // Sphere もインスタンス描画 (worldMat = scale(r) * translate(c))
  InstancedSphere colliderSpheres_;
  // Capsule は単位形状化が複雑 (start/end が可変) なので既存 Line を継続。
  YoRigine::Line colliderLineCapsule_;
  bool showColliderDebug_ = true; // コライダー表示フラグ

#ifdef USE_IMGUI
  bool showColliderSelectedOnly_ = false;  // 選択中のみ描画
  bool showBroadPhaseGrid_ = false;        // BroadPhase グリッド可視化
  float broadPhaseGridDrawRadius_ = 30.0f; // カメラからの可視化半径
#endif

  // ── Frustum culling ──────────────────────────────
  bool enableDrawFrustumCulling_ = true; // 描画でカリングするか
  float drawBoundsScaleFactor_ =
      2.0f; // スケール → 半サイズ係数 (コライダー無いとき)

  std::vector<int> copyObjectIDs_;
  Vector3 offsetCopyPos_ = {1.0f, 0.0f, 0.0f};

  SceneSerializer serializer_;
  PrefabManager prefabMgr_;
  ObjectSelector selector_;

#ifdef USE_IMGUI
  ModelBrowser browser_;
  SceneEditorUI editorUI_;
  StampMode stampMode_;
  GizmoController gizmoCtrl_;
  std::vector<PlacedObjectGizmable> gizmables_;
#endif
};

} // namespace YoRigine