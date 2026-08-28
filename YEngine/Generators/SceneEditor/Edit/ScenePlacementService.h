#pragma once

// Engine
#include "../Core/SceneEditorContext.h"
// PlacedObject は ObjectManager のネスト型なので前方宣言できない
#include <Object3D/ObjectManager.h>

// C++
#include <string>

namespace YoRigine {

/// <summary>
/// 「オブジェクトを置く / 揃える」操作をまとめたクラス。
///
/// モデルファイルからの配置、地面への吸着、グリッドへの整列など、
/// メニュー・ショートカット・モデルブラウザのどこから呼ばれても
/// 同じ結果になるように 1 箇所へ集約している。
/// </summary>
class ScenePlacementService {
public:
  explicit ScenePlacementService(const SceneEditorContext &context)
      : context_(context) {}

  void SetModelFolderPath(const std::string &path) { modelFolderPath_ = path; }
  const std::string &GetModelFolderPath() const { return modelFolderPath_; }

  // モデルファイルをシーンに配置し、そのオブジェクトを選択状態にする。
  // 失敗時は nullptr。
  ObjectManager::PlacedObject *PlaceModel(const std::string &modelPath);

  // 選択中オブジェクトを真下へ Raycast して地面/物体表面に吸着させる。
  // 吸着できた個数を返す。
  int SnapSelectionToSurface();

  // 選択中オブジェクトの位置を gridSize 単位に丸める。
  int SnapSelectionToGrid(float gridSize);

  // 選択中オブジェクトの回転を stepDegrees 単位に丸める。
  int SnapSelectionRotation(float stepDegrees);

private:
  const SceneEditorContext &context_;
  std::string modelFolderPath_ = "Resources/Models/";
};

} // namespace YoRigine
