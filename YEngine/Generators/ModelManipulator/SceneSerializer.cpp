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
            j["version"] = 3;
            j["objects"] = json::array();

            j["colliderTemplates"] = json::object();
            for (const auto& [modelName, tmpl] : objectManager_->GetColliderTemplates()) {
                j["colliderTemplates"][modelName] = {
                    {"typeId", static_cast<uint32_t>(tmpl.typeId)},
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
                };

                if (obj->collider) {
                    if (const auto* aabb = dynamic_cast<const AABBCollider*>(obj->collider.get())) {
                        objJson["aabbMin"] = { aabb->aabbOffset_.min.x, aabb->aabbOffset_.min.y, aabb->aabbOffset_.min.z };
                        objJson["aabbMax"] = { aabb->aabbOffset_.max.x, aabb->aabbOffset_.max.y, aabb->aabbOffset_.max.z };
                    }
                }

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
            if (version < 1 || version > 3) return false;

            objectManager_->ClearAllObjects();

            // colliderTemplates 復元（typeId のみ。version 2 の aabbOffset_ は後段でオブジェクトへ）
            if (j.contains("colliderTemplates")) {
                for (const auto& [modelName, tmplJson] : j["colliderTemplates"].items()) {
                    auto& tmpl = objectManager_->GetOrCreateTemplate(modelName);
                    tmpl.typeId = static_cast<CollisionTypeIdDef>(tmplJson.value("typeId", 0u));
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

                objectManager_->ApplyColliderTemplate(*obj);

                // AABB オフセット復元
                if (obj->collider) {
                    auto* aabb = dynamic_cast<AABBCollider*>(obj->collider.get());
                    if (aabb) {
                        if (version >= 3 && o.contains("aabbMin") && o.contains("aabbMax")) {
                            // version 3: インスタンスごとの値を直接復元
                            aabb->aabbOffset_.min = { o["aabbMin"][0], o["aabbMin"][1], o["aabbMin"][2] };
                            aabb->aabbOffset_.max = { o["aabbMax"][0], o["aabbMax"][1], o["aabbMax"][2] };
                        }
                        else if (version == 2 && j.contains("colliderTemplates")
                            && j["colliderTemplates"].contains(obj->modelName)) {
                            // version 2 後方互換: テンプレートの値をインスタンスへ移送
                            const auto& tmplJson = j["colliderTemplates"][obj->modelName];
                            if (tmplJson.contains("minScale"))
                                aabb->aabbOffset_.min = { tmplJson["minScale"][0], tmplJson["minScale"][1], tmplJson["minScale"][2] };
                            if (tmplJson.contains("maxScale"))
                                aabb->aabbOffset_.max = { tmplJson["maxScale"][0], tmplJson["maxScale"][1], tmplJson["maxScale"][2] };
                        }
                        // version 1 / テンプレートなし: AABBCollider::Initialize() のデフォルト値のまま
                    }
                }
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
            j["version"] = 3;  // SaveScene と同じバージョンに統一
            j["objects"] = json::array();

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
                    {"colliderEnabled", obj->colliderEnabled},  // 追加
                };

                // AABB オフセットを保存（SaveScene と同じ処理）
                if (obj->collider) {
                    if (const auto* aabb = dynamic_cast<const AABBCollider*>(obj->collider.get())) {
                        objJson["aabbMin"] = { aabb->aabbOffset_.min.x, aabb->aabbOffset_.min.y, aabb->aabbOffset_.min.z };
                        objJson["aabbMax"] = { aabb->aabbOffset_.max.x, aabb->aabbOffset_.max.y, aabb->aabbOffset_.max.z };
                    }
                }

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

                // version 3: コライダーデータを復元（LoadScene と同じ処理）
                if (version >= 3) {
                    obj->colliderEnabled = o.value("colliderEnabled", false);

                    // コライダーテンプレートを適用（なければ自動生成）
                    objectManager_->ApplyColliderTemplate(*obj);

                    // AABB オフセットをインスタンスへ直接復元
                    if (obj->collider) {
                        auto* aabb = dynamic_cast<AABBCollider*>(obj->collider.get());
                        if (aabb && o.contains("aabbMin") && o.contains("aabbMax")) {
                            aabb->aabbOffset_.min = { o["aabbMin"][0], o["aabbMin"][1], o["aabbMin"][2] };
                            aabb->aabbOffset_.max = { o["aabbMax"][0], o["aabbMax"][1], o["aabbMax"][2] };
                        }
                    }
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