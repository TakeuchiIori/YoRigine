#pragma once

#ifdef USE_IMGUI

// Engine
#include "ScenePanelContext.h"

// Math
#include "Vector3.h"

namespace YoRigine {

/// <summary>
/// 複製ツールパネル。
///
/// 選択中オブジェクトを一定間隔で並べて複製する。柵・柱・階段など
/// 等間隔に並ぶものを手で置かずに済ませるための道具。
/// </summary>
class DuplicatePanel {
public:
  void Draw(const ScenePanelContext &context, bool *isOpen);

private:
  void Execute(const ScenePanelContext &context);

  Vector3 offset_ = {1.0f, 0.0f, 0.0f};
  int count_ = 1;
  bool keepParent_ = false;
  // 選択中すべてを対象にするか (false なら主選択のみ)
  bool applyToAllSelected_ = false;
};

} // namespace YoRigine

#endif // USE_IMGUI
