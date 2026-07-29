#pragma once

#ifdef USE_IMGUI

// Engine
#include "ScenePanelContext.h"
#include <Core/Editor/Widgets/YEditorWidget.h>

// C++
#include <string>

namespace YoRigine {

/// <summary>
/// プレファブパネル。
///
/// 選択オブジェクト (またはシーン全体) をプレファブとして保存し、
/// 一覧から配置・削除する。JSON の読み書き自体は PrefabManager 経由。
/// </summary>
class PrefabPanel {
public:
  void Draw(const ScenePanelContext &context, bool *isOpen);

private:
  void DrawCreateSection(const ScenePanelContext &context);
  void DrawLibrarySection(const ScenePanelContext &context);

  YEditorWidget::SearchBox searchBox_;
  std::string selectedPrefabName_;
  char newPrefabName_[64] = {};
};

} // namespace YoRigine

#endif // USE_IMGUI
