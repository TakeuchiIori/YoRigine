#include "SceneSerializer.h"
#include "SceneJsonBinding.h"

// Engine
#include <Collision/Core/CollisionManager.h>
#include <Debugger/Logger.h>
#include <Material/MaterialOverrideSet.h>

// C++
#include <filesystem>
#include <fstream>
#include <unordered_map>

using json = nlohmann::json;

namespace YoRigine {

namespace {
// マテリアル上書きを収めるキー
constexpr const char *kMaterialOverridesKey = "materialOverrides";
constexpr const char *kSlotIndexKey = "slot";
} // namespace

//=============================================================================
// シーン設定
//=============================================================================
void SceneSerializer::WriteSceneSettings(json &root) const {
  if (!viewSettings_) {
    return;
  }
  bool collisionFrustumCulling =
      CollisionManager::GetInstance()->GetEnableFrustumCulling();
  SceneJsonBinding::BindSceneSettings(*viewSettings_, collisionFrustumCulling)
      .Save(root["sceneSettings"]);
}

void SceneSerializer::ReadSceneSettings(const json &root) {
  if (!viewSettings_ || !root.contains("sceneSettings")) {
    return;
  }
  auto *collisionManager = CollisionManager::GetInstance();
  bool collisionFrustumCulling = collisionManager->GetEnableFrustumCulling();

  SceneJsonBinding::BindSceneSettings(*viewSettings_, collisionFrustumCulling)
      .Load(root["sceneSettings"]);

  collisionManager->SetEnableFrustumCulling(collisionFrustumCulling);
}

//=============================================================================
// 1 オブジェクトの書き出し
//=============================================================================
json SceneSerializer::WriteObject(ObjectManager::PlacedObject &obj) const {
  json out;
  SceneJsonBinding::BindPlacedObject(obj).Save(out);

  // メッシュ単位のマテリアル上書き。何も上書きしていなければキーごと省く。
  MaterialOverrideSet *overrides =
      objectManager_ ? objectManager_->GetMaterialOverrides(obj) : nullptr;
  if (!overrides || !overrides->HasAnyOverride()) {
    return out;
  }

  out[kMaterialOverridesKey] = json::array();
  auto &slots = overrides->GetSlots();
  for (size_t i = 0; i < slots.size(); ++i) {
    if (!slots[i].IsActive()) {
      continue; // 未設定のスロットは保存しない (ファイルが無駄に膨らむため)
    }
    json slotJson;
    slotJson[kSlotIndexKey] = static_cast<int>(i);
    SceneJsonBinding::BindMaterialOverride(slots[i]).Save(slotJson);
    out[kMaterialOverridesKey].push_back(std::move(slotJson));
  }
  return out;
}

//=============================================================================
// 1 オブジェクトの読み込み
//=============================================================================
ObjectManager::PlacedObject *SceneSerializer::ReadObject(const json &source) {
  auto *obj = objectManager_->CreateObject(
      source.value("filePath", std::string{}),
      source.value("isAnimation", false),
      source.value("animationName", std::string{}));
  if (!obj) {
    return nullptr;
  }

  // CreateObject が採番した ID を上書きされないよう、読み込み後に復元する
  const int assignedId = obj->id;
  SceneJsonBinding::BindPlacedObject(*obj).Load(source);
  const int savedId = obj->id;
  obj->id = assignedId;

  objectManager_->ApplyObjectColor(*obj);
  objectManager_->ApplyObjectUV(*obj);
  objectManager_->ApplyColliderTemplate(*obj);

  // ── マテリアル上書き ──
  if (source.contains(kMaterialOverridesKey)) {
    MaterialOverrideSet *overrides =
        objectManager_->GetOrCreateMaterialOverrides(*obj);
    if (overrides) {
      for (const auto &slotJson : source[kMaterialOverridesKey]) {
        const size_t slotIndex =
            static_cast<size_t>(slotJson.value(kSlotIndexKey, 0));
        overrides->EnsureSlotCount(slotIndex + 1);
        MeshMaterialOverride *slot = overrides->GetSlot(slotIndex);
        if (!slot) {
          continue;
        }
        SceneJsonBinding::BindMaterialOverride(*slot).Load(slotJson);
        // テクスチャは TextureManager
        // への登録が要るのでセッター経由で入れ直す
        overrides->SetSlotTexture(slotIndex, slot->texturePath);
      }
      overrides->MarkDirty();
    }
  }

  // 親 ID の再マッピングのため、保存時の ID を呼び出し側へ返す必要がある。
  // ここでは obj->parentID に保存時の親 ID が入ったままなので、
  // ResolveHierarchy が oldToNewId を使って解決する。
  (void)savedId;
  return obj;
}

//=============================================================================
// 階層の解決
//
// ApplyColliderTemplate は読み込みループ内で先に走るが、その時点では
// UpdateMatrix 前で matWorld_ が原点のままのため AABB が原点付近に張り付く
// (= NavGrid::Bake が障害物を認識せず敵が貫通する原因)。
// トランスフォーム確定後にもう一度 collider->Update()
// を回して位置を反映させる。
//=============================================================================
void SceneSerializer::ResolveHierarchy(
    const std::unordered_map<int, int> &oldToNewId,
    bool remapThroughSetParent) {
  for (auto *obj : objectManager_->GetAllActiveObjects()) {
    if (obj->parentID != -1) {
      const auto it = oldToNewId.find(obj->parentID);
      if (remapThroughSetParent) {
        if (it != oldToNewId.end()) {
          objectManager_->SetParent(obj->id, it->second);
        }
      } else {
        obj->parentID = (it != oldToNewId.end()) ? it->second : -1;
      }
    }
    objectManager_->UpdateObjectTransform(*obj);
    if (obj->collider) {
      obj->collider->Update();
    }
  }
}

//=============================================================================
// シーン保存
//=============================================================================
bool SceneSerializer::SaveScene(const std::string &filePath) {
  if (!objectManager_) {
    return false;
  }
  try {
    json root;
    root["version"] = kCurrentVersion;
    WriteSceneSettings(root);

    root["objects"] = json::array();
    for (auto *obj : objectManager_->GetAllActiveObjects()) {
      if (!obj || !obj->object) {
        continue;
      }
      root["objects"].push_back(WriteObject(*obj));
    }

    const std::filesystem::path path(filePath);
    if (path.has_parent_path()) {
      std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(filePath);
    if (!file.is_open()) {
      Logger("[SceneSerializer] 保存用ファイルを開けません: " + filePath);
      return false;
    }
    file << root.dump(4);
    return true;
  } catch (const std::exception &e) {
    Logger(std::string("[SceneSerializer] SaveScene 失敗: ") + e.what());
    return false;
  }
}

//=============================================================================
// シーン読み込み
//=============================================================================
bool SceneSerializer::LoadScene(const std::string &filePath) {
  if (!objectManager_) {
    return false;
  }
  try {
    std::ifstream file(filePath);
    if (!file.is_open()) {
      return false;
    }

    json root;
    file >> root;

    const int version = root.value("version", 1);
    if (version < 1 || version > kCurrentVersion) {
      Logger("[SceneSerializer] 未知のバージョンです: " +
             std::to_string(version));
      return false;
    }

    ReadSceneSettings(root);
    objectManager_->ClearAllObjects();

    // 旧形式は互換パスへ。次の保存で自動的に新形式へ移行する。
    if (version < kAutoJsonVersion) {
      const bool ok = LoadLegacyObjects(root, version);
      Logger(ok ? "[SceneSerializer] 旧形式を読み込みました (v" +
                      std::to_string(version) + "): " + filePath
                : "[SceneSerializer] 旧形式の読み込みに失敗: " + filePath);
      return ok;
    }

    std::unordered_map<int, int> oldToNewId;
    for (const auto &objectJson : root["objects"]) {
      const int savedId = objectJson.value("id", -1);
      auto *obj = ReadObject(objectJson);
      if (!obj) {
        continue;
      }
      oldToNewId[savedId] = obj->id;
    }

    ResolveHierarchy(oldToNewId, /*remapThroughSetParent=*/false);

    Logger("[SceneSerializer] 読み込みました: " + filePath);
    return true;
  } catch (const std::exception &e) {
    Logger(std::string("[SceneSerializer] LoadScene 失敗: ") + e.what());
    return false;
  }
}

//=============================================================================
// プレファブ保存
//=============================================================================
bool SceneSerializer::SavePrefab(
    const std::vector<ObjectManager::PlacedObject *> &objects,
    const std::string &filePath) {
  try {
    json root;
    root["version"] = kCurrentVersion;
    root["objects"] = json::array();

    for (auto *obj : objects) {
      if (!obj) {
        continue;
      }
      root["objects"].push_back(WriteObject(*obj));
    }

    std::filesystem::create_directories(
        std::filesystem::path(filePath).parent_path());

    std::ofstream file(filePath);
    if (!file.is_open()) {
      return false;
    }
    file << root.dump(4);
    Logger("[SceneSerializer] プレファブを保存しました: " + filePath);
    return true;
  } catch (const std::exception &e) {
    Logger(std::string("[SceneSerializer] SavePrefab 失敗: ") + e.what());
    return false;
  }
}

//=============================================================================
// プレファブ読み込み
//
// シーンと違い、既存オブジェクトは消さずに追加する。
//=============================================================================
bool SceneSerializer::LoadPrefab(const std::string &filePath) {
  if (!objectManager_) {
    return false;
  }
  try {
    std::ifstream file(filePath);
    if (!file.is_open()) {
      return false;
    }

    json root;
    file >> root;

    const int version = root.value("version", 1);
    if (version < kAutoJsonVersion) {
      const bool ok = LoadLegacyObjects(root, version);
      Logger(ok ? "[SceneSerializer] 旧形式プレファブを読み込みました: " +
                      filePath
                : "[SceneSerializer] 旧形式プレファブの読み込みに失敗: " +
                      filePath);
      return ok;
    }

    std::unordered_map<int, int> oldToNewId;
    for (const auto &objectJson : root["objects"]) {
      const int savedId = objectJson.value("id", -1);
      auto *obj = ReadObject(objectJson);
      if (!obj) {
        continue;
      }
      oldToNewId[savedId] = obj->id;
    }

    ResolveHierarchy(oldToNewId, /*remapThroughSetParent=*/true);

    Logger("[SceneSerializer] プレファブを読み込みました: " + filePath);
    return true;
  } catch (const std::exception &e) {
    Logger(std::string("[SceneSerializer] LoadPrefab 失敗: ") + e.what());
    return false;
  }
}

} // namespace YoRigine
