#pragma once

#ifdef USE_IMGUI

// Engine
#include "ScenePanelContext.h"

namespace YoRigine {

/// <summary>
/// シーンエディタのメニューバー。
///
/// どのウィンドウを開くかのフラグは SceneEditorUI (パネルホスト) が持ち、
/// ここはその参照を受け取ってチェックを付け外しするだけ。
/// </summary>
class SceneMenuBar {
public:
  // ホスト側のウィンドウ表示フラグへの参照束。
  struct WindowFlags {
    bool *outliner = nullptr;
    bool *inspector = nullptr;
    bool *duplicate = nullptr;
    bool *prefab = nullptr;
    bool *colliderOverview = nullptr;
    bool *modelBrowser = nullptr;
  };

  void Draw(const ScenePanelContext &context, const WindowFlags &flags);

private:
  void DrawFileMenu(const ScenePanelContext &context);
  void DrawWindowMenu(const WindowFlags &flags);
  void DrawEditMenu(const ScenePanelContext &context);
  void DrawViewMenu(const ScenePanelContext &context);

  // 「グリッドに整列」で使う刻み幅
  float gridSnapSize_ = 1.0f;
  float rotationSnapDegrees_ = 15.0f;
};

} // namespace YoRigine

#endif // USE_IMGUI
