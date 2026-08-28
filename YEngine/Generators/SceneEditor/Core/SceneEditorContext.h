#pragma once

// Engine
#include "SceneViewSettings.h"

class ObjectManager;
class MotionEditor;

namespace YoRigine {

class Camera;
class ObjectSelector;
class PrefabManager;
class SceneSerializer;

/// <summary>
/// シーンエディタの各サブシステムが共通で必要とする借用ポインタ束。
///
/// SceneEditor が実体を所有し、サブシステムにはこの構造体への参照だけを渡す。
/// サブシステムごとに Set〜() を並べる必要が無くなり、依存が一目で分かる。
/// 所有はしないので、SceneEditor より長生きするサブシステムを作らないこと。
/// </summary>
struct SceneEditorContext {
  Camera *camera = nullptr;
  ObjectManager *objectManager = nullptr;
  ObjectSelector *selector = nullptr;
  SceneSerializer *serializer = nullptr;
  PrefabManager *prefabManager = nullptr;
  MotionEditor *motionEditor = nullptr;

  // SceneEditor が所有する表示設定への参照
  SceneViewSettings *viewSettings = nullptr;

  // 最低限の依存が揃っているか (描画・編集系はこれを見て早期 return する)
  bool IsValid() const {
    return objectManager != nullptr && viewSettings != nullptr;
  }
};

} // namespace YoRigine
