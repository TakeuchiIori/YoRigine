#include "SceneClipboard.h"

// Engine
#include "../ObjectSelector.h"
#include <Debugger/Logger.h>

namespace YoRigine {

void SceneClipboard::Copy() {
  if (!context_.IsValid() || !context_.selector) {
    return;
  }

  const auto &selectedIds = context_.selector->GetSelectedIds();
  if (selectedIds.empty()) {
    Logger("[SceneClipboard] コピー対象がありません (選択なし)");
    return;
  }

  copiedObjectIds_.assign(selectedIds.begin(), selectedIds.end());
  Logger("[SceneClipboard] " + std::to_string(copiedObjectIds_.size()) +
         " 件コピーしました");
}

void SceneClipboard::Paste() {
  if (!context_.IsValid() || !context_.selector) {
    return;
  }
  if (copiedObjectIds_.empty()) {
    Logger("[SceneClipboard] コピーバッファが空です");
    return;
  }

  ObjectManager *objectManager = context_.objectManager;

  // 貼り付けたものを新しい選択状態にする
  context_.selector->ClearSelection();

  int pastedCount = 0;
  for (const int sourceId : copiedObjectIds_) {
    auto *src = objectManager->GetObjectById(sourceId);
    if (!src) {
      continue;
    }

    auto *dst = objectManager->CreateObject(src->modelPath, src->isAnimation,
                                            src->animationName);
    if (!dst) {
      Logger("[SceneClipboard] CreateObject に失敗しました (元ID=" +
             std::to_string(sourceId) + ")");
      continue;
    }

    // ── トランスフォーム ──
    dst->position = src->position + pasteOffset_;
    dst->rotation = src->rotation;
    dst->scale = src->scale;
    dst->useAnchorPoint = src->useAnchorPoint;
    dst->anchorPoint = src->anchorPoint;

    // ── コライダー ──
    dst->colliderEnabled = src->colliderEnabled;
    dst->colliderCameraFade = src->colliderCameraFade;
    dst->colliderTypeId = src->colliderTypeId;
    dst->colliderShapeType = src->colliderShapeType;
    dst->colliderAabbOffset = src->colliderAabbOffset;
    dst->colliderObbCenter = src->colliderObbCenter;
    dst->colliderObbSize = src->colliderObbSize;
    dst->colliderObbEuler = src->colliderObbEuler;
    dst->colliderSphereCenter = src->colliderSphereCenter;
    dst->colliderSphereRadius = src->colliderSphereRadius;
    objectManager->ApplyColliderTemplate(*dst);

    // ── マテリアル / UV ──
    dst->color = src->color;
    objectManager->ApplyObjectColor(*dst);
    dst->uvScale = src->uvScale;
    dst->uvStochastic = src->uvStochastic;
    objectManager->ApplyObjectUV(*dst);
    objectManager->CopyMaterialOverrides(*src, *dst);

    // ── 描画フラグ / 階層 ──
    dst->outlineEnabled = src->outlineEnabled;
    dst->castShadow = src->castShadow;
    dst->pickable = src->pickable;
    dst->parentID = src->parentID;

    context_.selector->AddToSelection(dst->id);
    objectManager->UpdateObjectTransform(*dst);
    ++pastedCount;
  }

  Logger("[SceneClipboard] " + std::to_string(pastedCount) +
         " 件貼り付けました");
}

} // namespace YoRigine
