#pragma once

#ifdef USE_IMGUI

// Engine
#include "../Core/SceneEditorContext.h"
// PlacedObject は ObjectManager のネスト型なので前方宣言できない。
// パネルはどれも PlacedObject を触るためここで実体をインクルードする。
#include <Object3D/ObjectManager.h>

// C++
#include <functional>
#include <string>
#include <vector>

namespace YoRigine {

class SceneGizmoLayer;
class SceneLoadController;
class ScenePlacementService;

/// <summary>
/// 各パネルが共通で必要とする依存の束。
///
/// SceneEditorContext (シーン側の実体) に加えて、UI
/// からしか使わないサービスと 「SceneEditor
/// が所有していてパネルからは直接触れないもの」へのコールバックを持つ。
/// </summary>
struct ScenePanelContext {
  const SceneEditorContext *scene = nullptr;

  ScenePlacementService *placement = nullptr;
  SceneLoadController *loader = nullptr;
  SceneGizmoLayer *gizmoLayer = nullptr;

  // スタンプ配置モード (StampMode の実体は SceneEditor が持つ)
  std::function<void()> startStamp;
  std::function<void()> exitStamp;
  std::function<bool()> isStamping;

  bool IsValid() const { return scene != nullptr && scene->IsValid(); }
};

} // namespace YoRigine

#endif // USE_IMGUI
