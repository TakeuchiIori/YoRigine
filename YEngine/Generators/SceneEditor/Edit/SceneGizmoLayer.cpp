#ifdef USE_IMGUI

#include "SceneGizmoLayer.h"

// Engine
#include "../ObjectSelector.h"
#include <Debugger/Gizmo/IGizmable.h>
#include <Motion/Editor/MotionEditor.h>

namespace YoRigine {

void SceneGizmoLayer::Draw(const ImVec2 &viewportPos,
                           const ImVec2 &viewportSize) {
  if (!context_.IsValid() || !context_.camera || !context_.selector) {
    return;
  }
  if (!context_.selector->HasSelection()) {
    return;
  }

  gizmables_.clear();

  const auto &selectedIds = context_.selector->GetSelectedIds();
  // emplace_back の realloc で既存要素のポインタが無効化されるのを防ぐため、
  // targets へ &g を積む前に必要数を確保しておく。
  gizmables_.reserve(selectedIds.size());

  MotionEditor *motionEditor = context_.motionEditor;
  for (const int id : selectedIds) {
    // ボーン表示中の対象は MotionEditor 側のギズモが担当する
    if (motionEditor && motionEditor->IsDrawBone() &&
        id == motionEditor->GetTargetObjectId()) {
      continue;
    }
    auto *obj = context_.objectManager->GetObjectById(id);
    if (obj && obj->worldTransform) {
      gizmables_.emplace_back(obj, context_.objectManager);
    }
  }

  if (gizmables_.empty()) {
    return;
  }

  std::vector<IGizmable *> targets;
  targets.reserve(gizmables_.size());
  for (auto &gizmable : gizmables_) {
    targets.push_back(&gizmable);
  }

  gizmoController_.Draw(context_.camera, targets, viewportPos, viewportSize);
}

} // namespace YoRigine

#endif // USE_IMGUI
