#pragma once

// C++
#include <string>
#include <vector>

// Engine
#include "Core/SceneViewSettings.h"
#include <Object3D/ObjectManager.h>

// JSON
#include <json.hpp>

namespace YoRigine {

/// <summary>
/// シーン・プレファブの JSON Save/Load を一手に担うクラス。
///
/// 保存・読み込みのフィールド定義は AutoJson に一本化してある
/// (SceneJsonBinding)。 項目を増やすときは binding
/// に Add を 1 行足すだけで、Save 側 Load 側の 両方に自動で反映される
/// ——「片方だけ書き忘れて値が消える」事故を防ぐため。
///
/// ■ バージョン
///   version 15 以降 … AutoJson 形式 (Vector は {"x":..,"y":..} のオブジェクト)
///   version 14 以下 … 旧手書き形式 (Vector は [x,y,z] の配列)
/// 旧形式は読み込み専用の互換パス (SceneSerializerLegacy.cpp) で処理し、
/// 次の保存で自動的に新形式へ移行する。
/// </summary>
class SceneSerializer {
public:
  // 新規保存で書き出すフォーマットバージョン
  static constexpr int kCurrentVersion = 15;
  // AutoJson 形式が導入されたバージョン。これ未満は互換パスで読む。
  static constexpr int kAutoJsonVersion = 15;

  SceneSerializer() = default;
  ~SceneSerializer() = default;

  void SetObjectManager(ObjectManager *mgr) { objectManager_ = mgr; }
  void SetModelFolderPath(const std::string &path) { modelFolderPath_ = path; }
  void SetViewSettings(SceneViewSettings *settings) {
    viewSettings_ = settings;
  }

  // ── シーン ───────────────────────────────────────────────
  bool SaveScene(const std::string &filePath);
  bool LoadScene(const std::string &filePath);

  // ── プレファブ ───────────────────────────────────────────
  bool SavePrefab(const std::vector<ObjectManager::PlacedObject *> &objects,
                  const std::string &filePath);
  bool LoadPrefab(const std::string &filePath);

private:
  // 1 オブジェクトぶんの JSON を書き出す / 読み込む (シーンとプレファブで共用)
  nlohmann::json WriteObject(ObjectManager::PlacedObject &obj) const;
  ObjectManager::PlacedObject *ReadObject(const nlohmann::json &source);

  // オブジェクト配列を読み終えたあとの後処理
  // (親 ID の再マッピング + トランスフォーム確定 + コライダー再構築)
  void ResolveHierarchy(const std::unordered_map<int, int> &oldToNewId,
                        bool remapThroughSetParent);

  // 旧形式 (version <= 14) の読み込み。SceneSerializerLegacy.cpp に実装。
  bool LoadLegacyObjects(const nlohmann::json &root, int version);

  // シーン全体の設定 (カリングなど) の読み書き
  void WriteSceneSettings(nlohmann::json &root) const;
  void ReadSceneSettings(const nlohmann::json &root);

  ObjectManager *objectManager_ = nullptr;
  SceneViewSettings *viewSettings_ = nullptr;
  std::string modelFolderPath_ = "Resources/Models/";
};

} // namespace YoRigine
