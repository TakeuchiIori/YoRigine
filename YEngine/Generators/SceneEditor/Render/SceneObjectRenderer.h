#pragma once

// Engine
#include "../Core/SceneEditorContext.h"
#include <Object3D/ObjectManager.h>

// Math
#include "Shape/AABB.h"

namespace YoRigine {

/// <summary>
/// 配置済みオブジェクトの本描画を担うクラス。
///
/// SceneEditor から「何をどう描くか」を切り出したもの。
///   - 視錐台カリング
///   - 非アニメオブジェクトのインスタンシング集約
///   - カメラ遮蔽のディザーフェード
///   - シャドウマップパス
///   - ピックバッファ (ObjectID 焼き込み) パス
/// はすべてここに閉じている。
/// </summary>
class SceneObjectRenderer {
public:
  explicit SceneObjectRenderer(const SceneEditorContext &context)
      : context_(context) {}

  // カラーパス。インスタンシング可能なものはまとめて 1 ドローで描く。
  void Draw();

  // シャドウマップパス。castShadow=false のオブジェクトは描かない。
  void DrawShadow();

  // ピックパス。pickable=false のオブジェクトは ID を焼かない。
  // BeginPickPass / EndPickPass の間から呼ぶこと。
  void DrawForPick();

private:
  // 視錐台カリング用の外接ボックス。コライダーがあればそれを、
  // 無ければ position ± (scale * 係数) で大雑把に求める。
  AABB ComputeDrawBounds(const ObjectManager::PlacedObject &obj) const;

  const SceneEditorContext &context_;
};

} // namespace YoRigine
