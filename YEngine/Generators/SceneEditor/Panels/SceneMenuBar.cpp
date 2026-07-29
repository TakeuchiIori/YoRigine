#ifdef USE_IMGUI

#include "SceneMenuBar.h"

// Engine
#include "../Edit/SceneGizmoLayer.h"
#include "../Edit/SceneLoadController.h"
#include "../Edit/ScenePlacementService.h"
#include "../ObjectSelector.h"

namespace YoRigine {

//=============================================================================
// ファイル
//=============================================================================
void SceneMenuBar::DrawFileMenu(const ScenePanelContext &context) {
  if (!ImGui::BeginMenu("ファイル")) {
    return;
  }

  SceneLoadController *loader = context.loader;
  const bool hasScene = loader && !loader->GetCurrentScenePath().empty();

  if (!hasScene) {
    ImGui::BeginDisabled();
  }
  if (ImGui::MenuItem("保存", "Ctrl+S")) {
    loader->Save();
  }
  if (ImGui::MenuItem("読み込み直す")) {
    loader->Reload();
  }
  if (!hasScene) {
    ImGui::EndDisabled();
  }

  if (hasScene) {
    ImGui::Separator();
    ImGui::TextDisabled("%s", loader->GetCurrentScenePath().c_str());
  }

  ImGui::EndMenu();
}

//=============================================================================
// ウィンドウ
//=============================================================================
void SceneMenuBar::DrawWindowMenu(const WindowFlags &flags) {
  if (!ImGui::BeginMenu("ウィンドウ")) {
    return;
  }

  if (flags.outliner) {
    ImGui::MenuItem("アウトライナ", nullptr, flags.outliner);
  }
  if (flags.inspector) {
    ImGui::MenuItem("インスペクタ", nullptr, flags.inspector);
  }
  if (flags.modelBrowser) {
    ImGui::MenuItem("モデルブラウザ", nullptr, flags.modelBrowser);
  }
  ImGui::Separator();
  if (flags.duplicate) {
    ImGui::MenuItem("複製ツール", nullptr, flags.duplicate);
  }
  if (flags.prefab) {
    ImGui::MenuItem("プレファブ", nullptr, flags.prefab);
  }
  if (flags.colliderOverview) {
    ImGui::MenuItem("コライダー一覧", nullptr, flags.colliderOverview);
  }

  ImGui::EndMenu();
}

//=============================================================================
// 編集
//=============================================================================
void SceneMenuBar::DrawEditMenu(const ScenePanelContext &context) {
  if (!ImGui::BeginMenu("編集")) {
    return;
  }

  const bool hasSelection =
      context.scene->selector && context.scene->selector->HasSelection();

  if (!hasSelection) {
    ImGui::BeginDisabled();
  }

  if (ImGui::MenuItem("地面に吸着", "Ctrl+G") && context.placement) {
    context.placement->SnapSelectionToSurface();
  }
  if (ImGui::MenuItem("グリッドに整列") && context.placement) {
    context.placement->SnapSelectionToGrid(gridSnapSize_);
  }
  ImGui::SetNextItemWidth(120.0f);
  ImGui::DragFloat("  グリッド幅", &gridSnapSize_, 0.1f, 0.01f, 100.0f, "%.2f");

  if (ImGui::MenuItem("回転を整列") && context.placement) {
    context.placement->SnapSelectionRotation(rotationSnapDegrees_);
  }
  ImGui::SetNextItemWidth(120.0f);
  ImGui::DragFloat("  回転刻み (度)", &rotationSnapDegrees_, 0.5f, 1.0f, 90.0f,
                   "%.1f");

  ImGui::Separator();

  // スタンプ配置: 選択オブジェクトをクリック連打で連続配置する
  const bool stamping = context.isStamping && context.isStamping();
  if (stamping) {
    if (ImGui::MenuItem("スタンプ配置を終了", "Esc / 右クリック") &&
        context.exitStamp) {
      context.exitStamp();
    }
  } else {
    if (ImGui::MenuItem("スタンプ配置を開始", "B") && context.startStamp) {
      context.startStamp();
    }
  }

  if (!hasSelection) {
    ImGui::EndDisabled();
  }

  // ── ギズモのスナップ設定 ──
  if (context.gizmoLayer) {
    ImGui::Separator();
    GizmoController &gizmo = context.gizmoLayer->GetController();

    bool useSnap = gizmo.IsUsingSnap();
    if (ImGui::MenuItem("ギズモのグリッドスナップ", nullptr, useSnap)) {
      gizmo.SetUseSnap(!useSnap);
    }

    Vector3 snapValues = gizmo.GetSnapValues();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::DragFloat3("  移動スナップ", &snapValues.x, 0.1f, 0.1f, 100.0f,
                          "%.2f")) {
      gizmo.SetSnapValues(snapValues);
    }

    float rotationSnap = gizmo.GetRotationSnap();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::DragFloat("  回転スナップ (度)", &rotationSnap, 0.5f, 0.5f, 90.0f,
                         "%.1f")) {
      gizmo.SetRotationSnap(rotationSnap);
    }
  }

  ImGui::EndMenu();
}

//=============================================================================
// 表示
//=============================================================================
void SceneMenuBar::DrawViewMenu(const ScenePanelContext &context) {
  if (!ImGui::BeginMenu("表示")) {
    return;
  }

  SceneViewSettings &settings = *context.scene->viewSettings;

  ImGui::MenuItem("選択中を枠で囲む", nullptr, &settings.showSelectionOutline);

  ImGui::Separator();
  ImGui::MenuItem("コライダーを表示", nullptr, &settings.showCollider);
  if (settings.showCollider) {
    ImGui::MenuItem("  選択中のみ", nullptr, &settings.showColliderSelectedOnly);
    ImGui::MenuItem("  BroadPhase グリッド", nullptr,
                    &settings.showBroadPhaseGrid);
    if (settings.showBroadPhaseGrid) {
      ImGui::SetNextItemWidth(120.0f);
      ImGui::DragFloat("  描画半径", &settings.broadPhaseGridDrawRadius, 1.0f,
                       5.0f, 500.0f, "%.1f");
    }
  }

  ImGui::Separator();
  ImGui::MenuItem("描画 Frustum カリング", nullptr,
                  &settings.enableDrawFrustumCulling);
  ImGui::Text("  カリング済み %d / %d",
              context.scene->objectManager->GetLastFrameCulledCount(),
              context.scene->objectManager->GetLastFrameTotalCount());

  ImGui::EndMenu();
}

//=============================================================================
// メニューバー本体
//=============================================================================
void SceneMenuBar::Draw(const ScenePanelContext &context,
                        const WindowFlags &flags) {
  if (!context.IsValid()) {
    return;
  }
  if (!ImGui::BeginMenu("シーンオブジェクト")) {
    return;
  }

  DrawFileMenu(context);
  DrawWindowMenu(flags);
  DrawEditMenu(context);
  DrawViewMenu(context);

  ImGui::EndMenu();
}

} // namespace YoRigine

#endif // USE_IMGUI
