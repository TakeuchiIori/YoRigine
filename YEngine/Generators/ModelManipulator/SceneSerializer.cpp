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
            j["version"] = 4;
            j["objects"] = json::array();

            j["colliderTemplates"] = json::object();
            for (const auto& [modelName, tmpl] : objectManager_->GetColliderTemplates()) {
                j["colliderTemplates"][modelName] = {
                    {"typeId",  static_cast<uint32_t>(tmpl.typeId)},
                    {"aabbMin", {tmpl.aabbOffset.min.x, tmpl.aabbOffset.min.y, tmpl.aabbOffset.min.z}},
                    {"aabbMax", {tmpl.aabbOffset.max.x, tmpl.aabbOffset.max.y, tmpl.aabbOffset.max.z}},
                };
            }

            for (const auto* obj : objectManager_->GetAllActiveObjects()) {
                if (!obj || !obj->object) continue;

                json objJson = {
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
                    // aabbMin/aabbMax は version 4 以降 colliderTemplates に移動
                };

                j["objects"].push_back(objJson);
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
            if (version < 1 || version > 4) return false;

            objectManager_->ClearAllObjects();

            // colliderTemplates 復元（version 4: typeId + aabbOffset。version 1-3: typeId のみ）
            if (j.contains("colliderTemplates")) {
                for (const auto& [modelName, tmplJson] : j["colliderTemplates"].items()) {
                    auto& tmpl = objectManager_->GetOrCreateTemplate(modelName);
                    tmpl.typeId = static_cast<CollisionTypeIdDef>(tmplJson.value("typeId", 0u));
                    if (tmplJson.contains("aabbMin")) {
                        tmpl.aabbOffset.min = { tmplJson["aabbMin"][0], tmplJson["aabbMin"][1], tmplJson["aabbMin"][2] };
                    }
                    if (tmplJson.contains("aabbMax")) {
                        tmpl.aabbOffset.max = { tmplJson["aabbMax"][0], tmplJson["aabbMax"][1], tmplJson["aabbMax"][2] };
                    }
                }
            }

            std::unordered_map<int, int> oldToNewId;

            // オブジェクト生成 ＋ aabbOffset_ 復元を同一ループで処理
            for (const auto& o : j["objects"]) {
                auto* obj = objectManager_->CreateObject(
                    o["filePath"].get<std::string>(),
                    o.value("isAnimation", false),
                    o.value("animationName", ""));
                if (!obj) continue;

                oldToNewId[o["id"].get<int>()] = obj->id;

                obj->position = { o["position"][0], o["position"][1], o["position"][2] };
                obj->rotation = { o["rotate"][0],   o["rotate"][1],   o["rotate"][2] };
                obj->scale = { o["scale"][0],    o["scale"][1],    o["scale"][2] };
                obj->colliderEnabled = o.value("colliderEnabled", false);
                if (o.contains("parentID")) obj->parentID = o["parentID"].get<int>();

                // version 3 後方互換: per-object AABB をテンプレートに昇格してから適用
                // （同名モデルは最後に読んだ値がテンプレートになる）
                if (version == 3 && o.contains("aabbMin") && o.contains("aabbMax")) {
                    auto& tmpl = objectManager_->GetOrCreateTemplate(obj->modelName);
                    tmpl.aabbOffset.min = { o["aabbMin"][0], o["aabbMin"][1], o["aabbMin"][2] };
                    tmpl.aabbOffset.max = { o["aabbMax"][0], o["aabbMax"][1], o["aabbMax"][2] };
                }
                else if (version == 2 && j.contains("colliderTemplates")
                    && j["colliderTemplates"].contains(obj->modelName)) {
                    // version 2 後方互換: テンプレートの minScale/maxScale をテンプレートに昇格
                    const auto& tmplJson = j["colliderTemplates"][obj->modelName];
                    auto& tmpl = objectManager_->GetOrCreateTemplate(obj->modelName);
                    if (tmplJson.contains("minScale"))
                        tmpl.aabbOffset.min = { tmplJson["minScale"][0], tmplJson["minScale"][1], tmplJson["minScale"][2] };
                    if (tmplJson.contains("maxScale"))
                        tmpl.aabbOffset.max = { tmplJson["maxScale"][0], tmplJson["maxScale"][1], tmplJson["maxScale"][2] };
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
            j["version"] = 4;
            j["objects"] = json::array();

            // このプレファブに含まれるモデルのテンプレートを保存
            j["colliderTemplates"] = json::object();
            for (const auto* obj : objects) {
                if (!obj) continue;
                if (j["colliderTemplates"].contains(obj->modelName)) continue;
                const auto* tmpl = objectManager_->FindTemplate(obj->modelName);
                if (!tmpl) continue;
                j["colliderTemplates"][obj->modelName] = {
                    {"typeId",  static_cast<uint32_t>(tmpl->typeId)},
                    {"aabbMin", {tmpl->aabbOffset.min.x, tmpl->aabbOffset.min.y, tmpl->aabbOffset.min.z}},
                    {"aabbMax", {tmpl->aabbOffset.max.x, tmpl->aabbOffset.max.y, tmpl->aabbOffset.max.z}},
                };
            }

            for (const auto* obj : objects) {
                if (!obj) continue;

                json objJson = {
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
                    // aabbMin/aabbMax は version 4 以降 colliderTemplates に移動
                };

                j["objects"].push_back(objJson);
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

            // colliderTemplates 復元（version 4: typeId + aabbOffset）
            if (j.contains("colliderTemplates")) {
                for (const auto& [modelName, tmplJson] : j["colliderTemplates"].items()) {
                    auto& tmpl = objectManager_->GetOrCreateTemplate(modelName);
                    tmpl.typeId = static_cast<CollisionTypeIdDef>(tmplJson.value("typeId", 0u));
                    if (tmplJson.contains("aabbMin")) {
                        tmpl.aabbOffset.min = { tmplJson["aabbMin"][0], tmplJson["aabbMin"][1], tmplJson["aabbMin"][2] };
                    }
                    if (tmplJson.contains("aabbMax")) {
                        tmpl.aabbOffset.max = { tmplJson["aabbMax"][0], tmplJson["aabbMax"][1], tmplJson["aabbMax"][2] };
                    }
                }
            }

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

                if (version >= 3) {
                    obj->colliderEnabled = o.value("colliderEnabled", false);

                    // version 3 後方互換: per-object AABB をテンプレートに昇格
                    if (version == 3 && o.contains("aabbMin") && o.contains("aabbMax")) {
                        auto& tmpl = objectManager_->GetOrCreateTemplate(obj->modelName);
                        tmpl.aabbOffset.min = { o["aabbMin"][0], o["aabbMin"][1], o["aabbMin"][2] };
                        tmpl.aabbOffset.max = { o["aabbMax"][0], o["aabbMax"][1], o["aabbMax"][2] };
                    }

                    objectManager_->ApplyColliderTemplate(*obj);
                }
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