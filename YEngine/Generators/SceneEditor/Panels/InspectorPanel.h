#pragma once

#ifdef USE_IMGUI

// Engine
#include "ColliderPanel.h"
#include "MaterialPanel.h"
#include "ScenePanelContext.h"
#include <Core/Editor/Widgets/YEditorWidget.h>

namespace YoRigine {

/// <summary>
/// インスペクタパネル。
///
/// Unity の Inspector 相当。選択中オブジェクトの
///   [トランスフォーム] [レンダリング] [マテリアル] [コライダー] [階層]
/// をタブで切り替えて編集する。以前は 1 本の縦長リストにすべてが並んでいて
/// 目的の項目に辿り着けなかったため、タブで分けている。
///
/// 各タブの中身は専用パネル (MaterialPanel / ColliderPanel) と
/// Widgets のウィジェット (TransformFields など) に委譲していて、
/// このクラス自身はレイアウトと選択状態の面倒だけを見る。
/// </summary>
class InspectorPanel {
public:
  void Draw(const ScenePanelContext &context);

private:
  void DrawTransformTab(const ScenePanelContext &context,
                        ObjectManager::PlacedObject &obj);
  void DrawRenderingTab(const ScenePanelContext &context,
                        ObjectManager::PlacedObject &obj);
  void DrawHierarchyTab(const ScenePanelContext &context,
                        ObjectManager::PlacedObject &obj);

  // 上部に常時出す「名前 / モデル」ヘッダ
  void DrawHeader(ObjectManager::PlacedObject &obj);
  // モーションを持つモデルだけに出す「モーションエディタで編集」トグル
  void DrawMotionEditorToggle(const ScenePanelContext &context,
                              ObjectManager::PlacedObject &obj);

  YEditorWidget::TransformFields transformFields_;
  MaterialPanel materialPanel_;
  ColliderPanel colliderPanel_;
};

} // namespace YoRigine

#endif // USE_IMGUI
