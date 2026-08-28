#pragma once

// Engine
#include "../Core/SceneEditorContext.h"
#include <Graphics/Drawer/InstancedShape/InstancedCube.h>
#include <Graphics/Drawer/InstancedShape/InstancedSphere.h>
#include <Graphics/Drawer/LineManager/Line.h>

namespace YoRigine {

/// <summary>
/// シーンエディタのデバッグ線描画を担うクラス。
///
/// 描くもの:
///   - 選択中オブジェクトの外接ボックス (Blender 風のオレンジ枠)
///   - コライダー形状 (AABB / OBB / Sphere / Capsule)
///   - BroadPhase グリッド
///
/// AABB / OBB は InstancedCube、Sphere は InstancedSphere に集約するため
/// 形状が何個あってもドローコールは数本で済む。Capsule
/// だけは start/end が可変で 単位形状にできないため Line 描画のまま。
/// </summary>
class SceneDebugDrawer {
public:
  explicit SceneDebugDrawer(const SceneEditorContext &context)
      : context_(context) {}

  void Initialize();
  void SetCamera(Camera *camera);

  // 上記すべてを描く。表示設定が OFF のものは自動でスキップされる。
  void Draw();

private:
  void DrawSelectionOutline();
  void DrawColliders();
  void DrawBroadPhaseGrid();

  const SceneEditorContext &context_;

  InstancedCube colliderCubes_;
  InstancedSphere colliderSpheres_;
  Line colliderLineCapsule_;
};

} // namespace YoRigine
