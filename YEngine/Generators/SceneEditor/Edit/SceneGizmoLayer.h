#pragma once

#ifdef USE_IMGUI

// Engine
#include "../Core/SceneEditorContext.h"
#include "../PlacedObjectGizmable.h"
#include <Debugger/Gizmo/GizmoController.h>

// C++
#include <vector>

namespace YoRigine {

/// <summary>
/// 選択中オブジェクトへのギズモ表示を担うクラス。
///
/// 選択集合から IGizmable のリストを組み立てて GizmoController
/// に渡すだけだが、 「ボーン表示中の対象は除外する」「realloc
/// でポインタが無効化されないよう
/// 事前に容量を確保する」といった注意点があるので独立させている。
/// </summary>
class SceneGizmoLayer {
public:
  explicit SceneGizmoLayer(const SceneEditorContext &context)
      : context_(context) {}

  void Initialize() { gizmoController_.Initialize(); }

  // ビューポート矩形を渡してギズモを描く
  void Draw(const ImVec2 &viewportPos, const ImVec2 &viewportSize);

  GizmoController &GetController() { return gizmoController_; }

private:
  const SceneEditorContext &context_;

  GizmoController gizmoController_;
  // Draw のたびに作り直す一時リスト。メンバに持つのは毎フレームの
  // ヒープ確保を避けるため。
  std::vector<PlacedObjectGizmable> gizmables_;
};

} // namespace YoRigine

#endif // USE_IMGUI
