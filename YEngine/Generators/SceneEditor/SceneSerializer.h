#pragma once

// C++
#include <string>
#include <unordered_map>
#include <vector>

// Engine
#include <Object3D/ObjectManager.h>

namespace YoRigine {

/// <summary>
/// シーン・プレファブの JSON Save/Load を一手に担うクラス。
/// ObjectManager* を受け取って操作するため SceneEditor 以外からも使用可能。
/// </summary>
class SceneSerializer {
public:
  SceneSerializer() = default;
  ~SceneSerializer() = default;

  void SetObjectManager(ObjectManager *mgr) { objectManager_ = mgr; }
  void SetModelFolderPath(const std::string &path) { modelFolderPath_ = path; }
  void SetDrawFrustumCullingFlag(bool *flag) { drawFrustumCulling_ = flag; }

  // ── シーン ───────────────────────────────────────────────
  bool SaveScene(const std::string &filePath);
  bool LoadScene(const std::string &filePath);

  // ── プレファブ ───────────────────────────────────────────
  bool SavePrefab(const std::vector<ObjectManager::PlacedObject *> &objects,
                  const std::string &filePath);
  bool LoadPrefab(const std::string &filePath);

private:
  ObjectManager *objectManager_ = nullptr;
  bool *drawFrustumCulling_ = nullptr;
  std::string modelFolderPath_ = "Resources/Models/";
};

} // namespace YoRigine
