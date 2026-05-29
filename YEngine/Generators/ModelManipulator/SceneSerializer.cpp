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

    bool SceneSerializer::SaveScene(const std::string& filePath) {
        if (!objectManager_) return false;
        try {
            json j;
            j["version"] = 5;
            j["objects"] = json::array();

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
                    {"colliderEnabled", obj->colliderEnabled},
                    {"colliderTypeId",  static_cast<uint32_t>(obj->colliderTypeId)},
                    {"colliderAabbMin", {obj->colliderAabbOffset.min.x, obj->colliderAabbOffset.min.y, obj->colliderAabbOffset.min.z}},
                    {"colliderAabbMax", {obj->colliderAabbOffset.max.x, obj->colliderAabbOffset.max.y, obj->colliderAabbOffset.max.z}},
                });
            }

            std::ofstream file(filePath);
            if (!file.is_open()) return false;
            file << j.dump(4);
            return true;
        }
        catch (const std::exception& e) {
            std::cout << "[SceneSerializer] SaveScene error: " << e.what() << "\n";
            return false;
        }
    }

    bool SceneSerializer::LoadScene(const std::string& filePath) {
        if (!objectManager_) return false;
        try {
            std::ifstream file(filePath);
            if (!file.is_open()) return false;

            json j;
            file >> j;
            const int version = j.value("version", 1);
            if (version < 1 || version > 5) return false;

            objectManager_->ClearAllObjects();

            // version 1-4 の後方互換用: colliderTemplates から typeId/aabb をモデル名ごとに保持
            // version 5 以降は per-object フィールドを直接読む
            struct LegacyTmpl { CollisionTypeIdDef typeId; AABB aabb; };
            std::unordered_map<std::string, LegacyTmpl> legacyTemplates;

            if (version <= 4 && j.contains("colliderTemplates")) {
                for (const auto& [modelName, tmplJson] : j["colliderTemplates"].items()) {
                    LegacyTmpl lt;
                    lt.typeId = static_cast<CollisionTypeIdDef>(tmplJson.value("typeId", 0u));
                    lt.aabb.min = { -1.0f, -1.0f, -1.0f };
                    lt.aabb.max = {  1.0f,  1.0f,  1.0f };
                    if (tmplJson.contains("aabbMin"))
                        lt.aabb.min = { tmplJson["aabbMin"][0], tmplJson["aabbMin"][1], tmplJson["aabbMin"][2] };
                    if (tmplJson.contains("aabbMax"))
                        lt.aabb.max = { tmplJson["aabbMax"][0], tmplJson["aabbMax"][1], tmplJson["aabbMax"][2] };
                    legacyTemplates[modelName] = lt;
                }
            }

            std::unordered_map<int, int> oldToNewId;

            for (const auto& o : j["objects"]) {
                auto* obj = objectManager_->CreateObject(
                    o["filePath"].get<std::string>(),
                    o.value("isAnimation", false),
                    o.value("animationName", ""));
                if (!obj) continue;

                oldToNewId[o["id"].get<int>()] = obj->id;

                obj->position = { o["position"][0], o["position"][1], o["position"][2] };
                obj->rotation = { o["rotate"][0],   o["rotate"][1],   o["rotate"][2] };
                obj->scale    = { o["scale"][0],    o["scale"][1],    o["scale"][2] };
                obj->colliderEnabled = o.value("colliderEnabled", false);
                if (o.contains("parentID")) obj->parentID = o["parentID"].get<int>();

                if (version == 5) {
                    // version 5: per-object コライダー設定を直接読む
                    obj->colliderTypeId = static_cast<CollisionTypeIdDef>(o.value("colliderTypeId", 0u));
                    if (o.contains("colliderAabbMin"))
                        obj->colliderAabbOffset.min = { o["colliderAabbMin"][0], o["colliderAabbMin"][1], o["colliderAabbMin"][2] };
                    if (o.contains("colliderAabbMax"))
                        obj->colliderAabbOffset.max = { o["colliderAabbMax"][0], o["colliderAabbMax"][1], o["colliderAabbMax"][2] };
                }
                else {
                    // version 1-4 後方互換: テンプレートの設定を個別オブジェクトに適用
                    auto it = legacyTemplates.find(obj->modelName);
                    if (it != legacyTemplates.end()) {
                        obj->colliderTypeId     = it->second.typeId;
                        obj->colliderAabbOffset = it->second.aabb;
                    }
                    // version 3: per-object AABB が個別に保存されている場合はそちらを優先
                    if (version == 3 && o.contains("aabbMin") && o.contains("aabbMax")) {
                        obj->colliderAabbOffset.min = { o["aabbMin"][0], o["aabbMin"][1], o["aabbMin"][2] };
                        obj->colliderAabbOffset.max = { o["aabbMax"][0], o["aabbMax"][1], o["aabbMax"][2] };
                    }
                }

                objectManager_->ApplyColliderTemplate(*obj);
            }

            // 親子関係を新 ID で再マッピング ＆ トランスフォーム更新
            for (auto* obj : objectManager_->GetAllActiveObjects()) {
                if (obj->parentID != -1) {
                    auto it = oldToNewId.find(obj->parentID);
                    obj->parentID = (it != oldToNewId.end()) ? it->second : -1;
                }
                objectManager_->UpdateObjectTransform(*obj);
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
            j["version"] = 5;
            j["objects"] = json::array();

            for (const auto* obj : objects) {
                if (!obj) continue;

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
                    {"colliderEnabled", obj->colliderEnabled},
                    {"colliderTypeId",  static_cast<uint32_t>(obj->colliderTypeId)},
                    {"colliderAabbMin", {obj->colliderAabbOffset.min.x, obj->colliderAabbOffset.min.y, obj->colliderAabbOffset.min.z}},
                    {"colliderAabbMax", {obj->colliderAabbOffset.max.x, obj->colliderAabbOffset.max.y, obj->colliderAabbOffset.max.z}},
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

            const int version = j.value("version", 1);

            // version 1-4 後方互換: colliderTemplates から読む
            struct LegacyTmpl { CollisionTypeIdDef typeId; AABB aabb; };
            std::unordered_map<std::string, LegacyTmpl> legacyTemplates;

            if (version <= 4 && j.contains("colliderTemplates")) {
                for (const auto& [modelName, tmplJson] : j["colliderTemplates"].items()) {
                    LegacyTmpl lt;
                    lt.typeId = static_cast<CollisionTypeIdDef>(tmplJson.value("typeId", 0u));
                    lt.aabb.min = { -1.0f, -1.0f, -1.0f };
                    lt.aabb.max = {  1.0f,  1.0f,  1.0f };
                    if (tmplJson.contains("aabbMin"))
                        lt.aabb.min = { tmplJson["aabbMin"][0], tmplJson["aabbMin"][1], tmplJson["aabbMin"][2] };
                    if (tmplJson.contains("aabbMax"))
                        lt.aabb.max = { tmplJson["aabbMax"][0], tmplJson["aabbMax"][1], tmplJson["aabbMax"][2] };
                    legacyTemplates[modelName] = lt;
                }
            }

            std::unordered_map<int, int> oldToNewId;

            for (const auto& o : j["objects"]) {
                auto* obj = objectManager_->CreateObject(
                    o["filePath"].get<std::string>(),
                    o.value("isAnimation", false),
                    o.value("animationName", ""));
                if (!obj) continue;

                oldToNewId[o["id"].get<int>()] = obj->id;

                obj->position = { o["position"][0], o["position"][1], o["position"][2] };
                obj->rotation = { o["rotate"][0],   o["rotate"][1],   o["rotate"][2] };
                obj->scale    = { o["scale"][0],    o["scale"][1],    o["scale"][2] };
                if (o.contains("parentID")) obj->parentID = o["parentID"].get<int>();

                obj->colliderEnabled = o.value("colliderEnabled", false);

                if (version == 5) {
                    obj->colliderTypeId = static_cast<CollisionTypeIdDef>(o.value("colliderTypeId", 0u));
                    if (o.contains("colliderAabbMin"))
                        obj->colliderAabbOffset.min = { o["colliderAabbMin"][0], o["colliderAabbMin"][1], o["colliderAabbMin"][2] };
                    if (o.contains("colliderAabbMax"))
                        obj->colliderAabbOffset.max = { o["colliderAabbMax"][0], o["colliderAabbMax"][1], o["colliderAabbMax"][2] };
                }
                else {
                    auto it = legacyTemplates.find(obj->modelName);
                    if (it != legacyTemplates.end()) {
                        obj->colliderTypeId     = it->second.typeId;
                        obj->colliderAabbOffset = it->second.aabb;
                    }
                    if (version == 3 && o.contains("aabbMin") && o.contains("aabbMax")) {
                        obj->colliderAabbOffset.min = { o["aabbMin"][0], o["aabbMin"][1], o["aabbMin"][2] };
                        obj->colliderAabbOffset.max = { o["aabbMax"][0], o["aabbMax"][1], o["aabbMax"][2] };
                    }
                }

                objectManager_->ApplyColliderTemplate(*obj);
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