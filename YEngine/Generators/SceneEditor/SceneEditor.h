#pragma once

// C++
#include <memory>
#include <string>
#include <vector>

// Engine
#include <Motion/Editor/MotionEditor.h>
#include <Object3D/ObjectManager.h>
#include <Systems/Camera/Camera.h>

// サブシステム
#include "Core/SceneEditorContext.h"
#include "Core/SceneViewSettings.h"
#include "Edit/SceneClipboard.h"
#include "Edit/SceneLoadController.h"
#include "Edit/ScenePlacementService.h"
#include "ObjectSelector.h"
#include "PickBuffer.h"
#include "PrefabManager.h"
#include "Render/SceneDebugDrawer.h"
#include "Render/SceneObjectRenderer.h"
#include "SceneSerializer.h"

#ifdef USE_IMGUI
#include "Edit/SceneEditorShortcuts.h"
#include "Edit/SceneGizmoLayer.h"
#include "ModelBrowser.h"
#include "SceneEditorUI.h"
#include "StampMode.h"
#endif

namespace YoRigine {

/// <summary>
/// シーンエディタのファサード。
///
/// 実際の処理はすべてサブシステムが持ち、このクラスは
///   - サブシステムの所有と初期化
///   - 共有コンテキスト (SceneEditorContext) の構築
///   - シーン側 (BaseScene / MyGame) からの呼び出しの取り次ぎ
/// だけを行う。ロジックをここに書き足さないこと。追加したい機能は
/// Render/ Edit/ Panels/ のいずれかに置いて、ここからは呼ぶだけにする。
/// </summary>
class SceneEditor {
public:
  //=========================================================================
  // 基本
  //=========================================================================
  static SceneEditor *GetInstance();

  void Initialize();
  void Update();
  void Finalize();

  // ── 描画パス ─────────────────────────────────────────────
  void Draw();         // カラーパス
  void DrawLine();     // デバッグ線 (コライダー / 選択枠 / グリッド)
  void DrawShadow();   // シャドウマップ
  void DrawPickPass(); // ピック用 ObjectID の焼き込み
  void DrawForPick();  // ピックパスの中身 (PickBuffer の Begin/End 間で呼ぶ)
  void DrawImGui();    // エディタ UI
  void DrawGizmo();    // 選択中オブジェクトのギズモ

  // ── シーン操作 ───────────────────────────────────────────
  void LoadScene(const std::string &sceneName);
  void PlaceObject(const std::string &modelPath);

  void SetCamera(Camera *camera);

  //=========================================================================
  // サブシステムアクセッサ
  //=========================================================================
  SceneSerializer &GetSerializer() { return serializer_; }
  ObjectSelector &GetSelector() { return selector_; }
  MotionEditor &GetMotionEditor() { return motionEditor_; }
  SceneViewSettings &GetViewSettings() { return viewSettings_; }
  ScenePlacementService &GetPlacementService() { return placement_; }

#ifdef USE_IMGUI
  // シーンエディタ (オブジェクト選択・ギズモ) が有効かどうか。
  // 「モデル操作」ウィンドウが閉じている、または Editor 全体が非表示なら
  // false。
  bool IsSceneEditorActive() const;
#endif

private:
  //=========================================================================
  // 内部処理
  //=========================================================================
  void BuildContext();

#ifdef USE_IMGUI
  void SetupUI();
  void SetupShortcuts();
  // 選択中オブジェクトをビューポート中央に収める (F キー)
  void FocusSelection();
  // 選択中オブジェクトをまとめて削除する (Delete キー)
  void DeleteSelection();
  // 選択中オブジェクトをその場で複製する (Ctrl+D)
  void DuplicateSelection();
#endif

  //=========================================================================
  // シングルトン
  //=========================================================================
  SceneEditor();
  ~SceneEditor() = default;
  SceneEditor(const SceneEditor &) = delete;
  SceneEditor &operator=(const SceneEditor &) = delete;
  SceneEditor(SceneEditor &&) = delete;
  SceneEditor &operator=(SceneEditor &&) = delete;

  //=========================================================================
  // メンバ変数
  //=========================================================================
  bool isInitialized_ = false;

  // ── サブシステムが共有する状態 ────────────────────────────
  SceneViewSettings viewSettings_;
  SceneEditorContext context_;

  // ── 所有するサブシステム ─────────────────────────────────
  ObjectManager *objectManager_ = nullptr;
  Camera *camera_ = nullptr;

  MotionEditor motionEditor_;
  SceneSerializer serializer_;
  PrefabManager prefabManager_;
  ObjectSelector selector_;

  SceneObjectRenderer renderer_;
  SceneDebugDrawer debugDrawer_;
  SceneClipboard clipboard_;
  ScenePlacementService placement_;
  SceneLoadController loadController_;

#ifdef USE_IMGUI
  PickBuffer *pickBuffer_ = nullptr;

  ModelBrowser modelBrowser_;
  SceneEditorUI editorUI_;
  StampMode stampMode_;
  SceneGizmoLayer gizmoLayer_;
  SceneEditorShortcuts shortcuts_;
#endif
};

} // namespace YoRigine
