#pragma once

#ifdef USE_IMGUI

// Engine
#include "ScenePanelContext.h"

namespace YoRigine {

/// <summary>
/// コライダー設定パネル。
///
/// インスペクタ内に埋め込む「選択中オブジェクトのコライダー設定」と、
/// 単独ウィンドウとして開く「シーン全体のコライダー一覧」の 2 つを提供する。
/// </summary>
class ColliderPanel {
public:
  // インスペクタ内に埋め込む形の設定 UI
  void DrawInspectorSection(const ScenePanelContext &context,
                            ObjectManager::PlacedObject &obj);

  // シーン全体のコライダーを表形式で一覧する
  void DrawOverviewWindow(const ScenePanelContext &context, bool *isOpen);

private:
  void DrawShapeFields(const ScenePanelContext &context,
                       ObjectManager::PlacedObject &obj, bool &changed);

  // 自動フィット時のマージン (1.0 等倍 / 1.05 で 5% 拡大)
  float fitMargin_ = 1.05f;
};

} // namespace YoRigine

#endif // USE_IMGUI
