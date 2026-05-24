#include "SceneSerializer.h"

// C++
#include <fstream>
#include <filesystem>
#include <iostream>
#include <unordered_map>

// JSON
#include <json.hpp>

// Engine
#include <Collision/Core/CollisionTypeIdDef.h>

using json = nlohmann::json;

namespace YoRigine {

    //=============================================================================
    // シーン保存
    //=============================================================================
    bool SceneSerializer::SaveScene(const std::string& filePath)
    {
        if (!objectManager_) return false;

        try {
            json j;
            j["version"] = 2;
            j["objects"] = json::array();

            // コライダーテンプレートをモデル名キーで保存
            j["colliderTemplates"] = json::object();
            for (const auto& [modelName, tmpl] : objectManager_->GetColliderTemplates()) {
                j["colliderTemplates"][modelName] = {
                    {"typeId", static_cast<uint32_t>(tmpl.typeId)},
                    {"size",   {tmpl.size.x,   tmpl.size.y,   tmpl.size.z}},
                    {"offset", {tmpl.offset.x, tmpl.offset.y, tmpl.offset.z}},
                };
            }

            for (const auto* obj : objectManager_->GetAllActiveObjects()) {
                if (!obj || !obj->object) continue;

                j["objects"].push_back({
                    {"id",              obj->id},
                    {"filePath",        obj->modelPath},
                    {"modelName",       obj->modelName},
                    {"position",        {obj->position.x, obj->position.y, obj->position.z}},
                    {"rotate",          {obj->rotation.x, obj->rotation.y, obj->rotation.z}},
                    {"scale",           {obj->scale.x,    obj->scale.y,    obj->scale.z}},
                    {"parentID",        obj->parentID},
                    {"isAnimation",     obj->isAnimation},
                    {"animationName",   obj->animationName},
                    {"colliderEnabled", obj->colliderEnabled},  // インスタンス個別フラグ
                    });
            }

            std::ofstream file(filePath);
            if (!file.is_open()) {
                std::cout << "[SceneSerializer] Failed to open for save: " << filePath << "\n";
                return false;
            }
            file << j.dump(4);
            std::cout << "[SceneSerializer] Scene saved: " << filePath << "\n";
            return true;
        }
        catch (const std::exception& e) {
            std::cout << "[SceneSerializer] SaveScene error: " << e.what() << "\n";
            return false;
        }
    }

    //=============================================================================
    // シーン読み込み
    //=============================================================================
    bool SceneSerializer::LoadScene(const std::string& filePath)
    {
        if (!objectManager_) return false;

        try {
            std::ifstream file(filePath);
            if (!file.is_open()) {
                std::cout << "[SceneSerializer] File open failed: " << filePath << "\n";
                return false;
            }

            json j;
            file >> j;
            const int version = j.value("version", 1);
            if (version < 1 || version > 2) {
                std::cout << "[SceneSerializer] Unsupported version: " << version << "\n";
                return false;
            }

            objectManager_->ClearAllObjects();

            // version 2: コライダーテンプレートを先に復元
            if (version >= 2 && j.contains("colliderTemplates")) {
                for (const auto& [modelName, tmplJson] : j["colliderTemplates"].items()) {
                    auto& tmpl = objectManager_->GetOrCreateTemplate(modelName);
                    tmpl.typeId = static_cast<CollisionTypeIdDef>(tmplJson.value("typeId", 0u));
                    if (tmplJson.contains("size")) {
                        tmpl.size = { tmplJson["size"][0], tmplJson["size"][1], tmplJson["size"][2] };
                    }
                    if (tmplJson.contains("offset")) {
                        tmpl.offset = { tmplJson["offset"][0], tmplJson["offset"][1], tmplJson["offset"][2] };
                    }
                }
            }

            // 旧ID → 新ID のマッピング（親子関係の再構築用）
            std::unordered_map<int, int> oldToNewId;

            for (const auto& o : j["objects"]) {
                bool        isAnim = o.value("isAnimation", false);
                std::string animName = o.value("animationName", "");

                auto* obj = objectManager_->CreateObject(
                    o["filePath"].get<std::string>(), isAnim, animName);
                if (!obj) {
                    std::cout << "[SceneSerializer] Skip: " << o["filePath"] << "\n";
                    continue;
                }

                oldToNewId[o["id"].get<int>()] = obj->id;

                obj->position = { o["position"][0], o["position"][1], o["position"][2] };
                obj->rotation = { o["rotate"][0],   o["rotate"][1],   o["rotate"][2] };
                obj->scale = { o["scale"][0],     o["scale"][1],    o["scale"][2] };
                obj->colliderEnabled = o.value("colliderEnabled", false);

                if (o.contains("parentID"))
                    obj->parentID = o["parentID"].get<int>();
            }

            // 親子関係を新IDで再マッピング、コライダーテンプレートを反映
            for (auto* obj : objectManager_->GetAllActiveObjects()) {
                if (obj->parentID != -1) {
                    auto it = oldToNewId.find(obj->parentID);
                    if (it != oldToNewId.end())
                        objectManager_->SetParent(obj->id, it->second);
                    else
                        obj->parentID = -1;
                }
                objectManager_->UpdateObjectTransform(*obj);
                objectManager_->ApplyColliderTemplate(*obj); // テンプレートをコライダーに反映
            }

            std::cout << "[SceneSerializer] Scene loaded: " << filePath << "\n";
            return true;
        }
        catch (const std::exception& e) {
            std::cout << "[SceneSerializer] LoadScene error: " << e.what() << "\n";
            return false;
        }
    }

    //=============================================================================
    // プレファブ保存
    //=============================================================================
    bool SceneSerializer::SavePrefab(
        const std::vector<ObjectManager::PlacedObject*>& objects,
        const std::string& filePath)
    {
        try {
            json j;
            j["version"] = 1;
            j["objects"] = json::array();

            for (const auto* obj : objects) {
                if (!obj) continue;
                j["objects"].push_back({
                    {"id",            obj->id},
                    {"filePath",      obj->modelPath},
                    {"modelName",     obj->modelName},
                    {"position",      {obj->position.x, obj->position.y, obj->position.z}},
                    {"rotate",        {obj->rotation.x, obj->rotation.y, obj->rotation.z}},
                    {"scale",         {obj->scale.x,    obj->scale.y,    obj->scale.z}},
                    {"parentID",      obj->parentID},
                    {"isAnimation",   obj->isAnimation},
                    {"animationName", obj->animationName},
                    });
            }

            std::filesystem::create_directories(
                std::filesystem::path(filePath).parent_path());

            std::ofstream file(filePath);
            if (!file.is_open()) return false;
            file << j.dump(4);
            std::cout << "[SceneSerializer] Prefab saved: " << filePath << "\n";
            return true;
        }
        catch (const std::exception& e) {
            std::cout << "[SceneSerializer] SavePrefab error: " << e.what() << "\n";
            return false;
        }
    }

    //=============================================================================
    // プレファブ読み込み
    //=============================================================================
    bool SceneSerializer::LoadPrefab(const std::string& filePath)
    {
        if (!objectManager_) return false;

        try {
            std::ifstream file(filePath);
            if (!file.is_open()) return false;

            json j;
            file >> j;

            std::unordered_map<int, int> oldToNewId;

            for (const auto& o : j["objects"]) {
                bool        isAnim = o.value("isAnimation", false);
                std::string animName = o.value("animationName", "");

                auto* obj = objectManager_->CreateObject(
                    o["filePath"].get<std::string>(), isAnim, animName);
                if (!obj) continue;

                oldToNewId[o["id"].get<int>()] = obj->id;

                obj->position = { o["position"][0], o["position"][1], o["position"][2] };
                obj->rotation = { o["rotate"][0],   o["rotate"][1],   o["rotate"][2] };
                obj->scale = { o["scale"][0],     o["scale"][1],    o["scale"][2] };

                if (o.contains("parentID"))
                    obj->parentID = o["parentID"].get<int>();
            }

            for (auto* obj : objectManager_->GetAllActiveObjects()) {
                if (obj->parentID != -1) {
                    auto it = oldToNewId.find(obj->parentID);
                    if (it != oldToNewId.end())
                        objectManager_->SetParent(obj->id, it->second);
                }
                objectManager_->UpdateObjectTransform(*obj);
            }

            std::cout << "[SceneSerializer] Prefab loaded: " << filePath << "\n";
            return true;
        }
        catch (const std::exception& e) {
            std::cout << "[SceneSerializer] LoadPrefab error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace YoRigine