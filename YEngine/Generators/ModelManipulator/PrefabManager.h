#pragma once

// C++
#include <string>
#include <vector>

// Engine
#include <Object3D/ObjectManager.h>

// Forward
namespace YoRigine { class SceneSerializer; }

namespace YoRigine {

/// <summary>
/// プレファブファイルの一覧管理・作成・読み込み・削除を担う。
/// JSON の実際の読み書きは SceneSerializer に委譲する。
/// </summary>
class PrefabManager
{
public:
    PrefabManager() = default;
    ~PrefabManager() = default;

    void SetObjectManager(ObjectManager* mgr)      { objectManager_ = mgr; }
    void SetSerializer(SceneSerializer* serializer) { serializer_ = serializer; }
    void SetPrefabFolderPath(const std::string& p)  { prefabFolderPath_ = p; }

    // ── フォルダスキャン ──────────────────────────────────────
    void ScanPrefabFolder();

    // ── CRUD ─────────────────────────────────────────────────
    void CreatePrefabFromObject(const std::string& name, int objectId);
    void CreatePrefabFromAllObjects(const std::string& name);
    void LoadPrefab(const std::string& name);
    void DeletePrefab(const std::string& name);

    // ── アクセッサ ────────────────────────────────────────────
    const std::vector<std::string>& GetPrefabList() const { return prefabList_; }

private:
    ObjectManager*  objectManager_    = nullptr;
    SceneSerializer* serializer_      = nullptr;
    std::string     prefabFolderPath_ = "Resources/Json/Prefabs/";
    std::vector<std::string> prefabList_;
};

} // namespace YoRigine
