#pragma once

#ifdef USE_IMGUI

// Engine
#include "Panels/ColliderPanel.h"
#include "Panels/DuplicatePanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/OutlinerPanel.h"
#include "Panels/PrefabPanel.h"
#include "Panels/SceneMenuBar.h"
#include "Panels/ScenePanelContext.h"

namespace YoRigine {

/// <summary>
/// シーンエディタ UI のホスト。
///
/// 以前はこのクラスが全ウィンドウの描画コードを直接持っていた (600 行超)。
/// 現在は各ウィンドウを Panels/ 配下の専用クラスへ委譲し、ここは
///   - どのウィンドウを開いているかのフラグ
///   - パネルの所有と描画順
/// だけを持つ。中身を足すときは新しいパネルクラスを作ってここに 1
/// 行足すこと。
/// </summary>
class SceneEditorUI {
public:
  void SetContext(const ScenePanelContext &context) { context_ = context; }

  // メニューバー (Editor::RegisterMenuBar から呼ばれる)
  void DrawMenuBar();

  // 開いているウィンドウをまとめて描画する
  void DrawPanels();

  // ── ウィンドウ表示フラグ ──────────────────────────────────
  bool IsOutlinerShown() const { return showOutliner_; }
  bool *GetShowOutlinerPtr() { return &showOutliner_; }
  bool *GetShowInspectorPtr() { return &showInspector_; }
  bool *GetShowModelBrowserPtr() { return &showModelBrowser_; }

private:
  SceneMenuBar::WindowFlags BuildWindowFlags();

  ScenePanelContext context_;

  // ── パネル ────────────────────────────────────────────────
  SceneMenuBar menuBar_;
  OutlinerPanel outlinerPanel_;
  InspectorPanel inspectorPanel_;
  DuplicatePanel duplicatePanel_;
  PrefabPanel prefabPanel_;
  ColliderPanel colliderPanel_;

  // ── 表示状態 ──────────────────────────────────────────────
  // アウトライナとインスペクタは常用するので既定で開いておく。
  bool showOutliner_ = true;
  bool showInspector_ = true;
  bool showModelBrowser_ = true;
  bool showDuplicate_ = false;
  bool showPrefab_ = false;
  bool showColliderOverview_ = false;
};

} // namespace YoRigine

#endif // USE_IMGUI
