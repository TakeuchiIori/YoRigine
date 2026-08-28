#include "PrefabManager.h"
#include "SceneSerializer.h"

// C++
#include <filesystem>
#include <iostream>

namespace YoRigine {

void PrefabManager::ScanPrefabFolder()
{
    prefabList_.clear();

    if (!std::filesystem::exists(prefabFolderPath_)) {
        std::filesystem::create_directories(prefabFolderPath_);
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(prefabFolderPath_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            prefabList_.push_back(entry.path().stem().string());
    }
}

void PrefabManager::CreatePrefabFromObject(const std::string& name, int objectId)
{
    if (!objectManager_ || !serializer_) return;

    std::vector<ObjectManager::PlacedObject*> objects;
    objectManager_->CollectObjectHierarchy(objectId, objects);

    serializer_->SavePrefab(objects, prefabFolderPath_ + name + ".json");
    ScanPrefabFolder();
    std::cout << "[PrefabManager] Created: " << name << "\n";
}

void PrefabManager::CreatePrefabFromAllObjects(const std::string& name)
{
    if (!objectManager_ || !serializer_) return;

    auto objects = objectManager_->GetAllActiveObjects();
    serializer_->SavePrefab(objects, prefabFolderPath_ + name + ".json");
    ScanPrefabFolder();
    std::cout << "[PrefabManager] Created from all: " << name << "\n";
}

void PrefabManager::LoadPrefab(const std::string& name)
{
    if (!serializer_) return;
    serializer_->LoadPrefab(prefabFolderPath_ + name + ".json");
}

void PrefabManager::DeletePrefab(const std::string& name)
{
    std::string path = prefabFolderPath_ + name + ".json";
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
        ScanPrefabFolder();
        std::cout << "[PrefabManager] Deleted: " << name << "\n";
    }
}

} // namespace YoRigine
