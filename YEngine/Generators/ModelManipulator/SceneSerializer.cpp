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
            j["version"] = 12;
            j["objects"] = json::array();

            for (const auto* obj : objectManager_->GetAllActiveObjects()) {
                if (!obj || !obj->object) continue;

                j["objects"].push_back({
                    {"id",                  obj->id},
                    {"filePath",            obj->modelPath},
                    {"modelName",           obj->modelName},
                    {"nameTag",             obj->nameTag},
                    {"position",            {obj->position.x, obj->position.y, obj->position.z}},
                    {"rotate",              {obj->rotation.x, obj->rotation.y, obj->rotation.z}},
                    {"scale",               {obj->scale.x,    obj->scale.y,    obj->scale.z}},
                    {"useAnchorPoint",      obj->useAnchorPoint},
                    {"anchorPoint",         {obj->anchorPoint.x, obj->anchorPoint.y, obj->anchorPoint.z}},
                    {"color",               {obj->color.x,    obj->color.y,    obj->color.z,    obj->color.w}},
                    {"uvScale",             {obj->uvScale.x,  obj->uvScale.y}},
                    {"uvStochastic",        obj->uvStochastic},
                    {"outlineEnabled",      obj->outlineEnabled},
                    {"parentID",            obj->parentID},
                    {"isAnimation",         obj->isAnimation},
                    {"animationName",       obj->animationName},
                    {"pickable",            obj->pickable},
                    {"colliderEnabled",     obj->colliderEnabled},
                    {"colliderTypeId",      static_cast<uint32_t>(obj->colliderTypeId)},
                    {"colliderShapeType",   static_cast<uint32_t>(obj->colliderShapeType)},
                    {"colliderAabbMin",     {obj->colliderAabbOffset.min.x, obj->colliderAabbOffset.min.y, obj->colliderAabbOffset.min.z}},
                    {"colliderAabbMax",     {obj->colliderAabbOffset.max.x, obj->colliderAabbOffset.max.y, obj->colliderAabbOffset.max.z}},
                    {"colliderObbCenter",   {obj->colliderObbCenter.x,    obj->colliderObbCenter.y,    obj->colliderObbCenter.z}},
                    {"colliderObbSize",     {obj->colliderObbSize.x,      obj->colliderObbSize.y,      obj->colliderObbSize.z}},
                    {"colliderObbEuler",    {obj->colliderObbEuler.x,     obj->colliderObbEuler.y,     obj->colliderObbEuler.z}},
                    {"colliderSphCenter",   {obj->colliderSphereCenter.x, obj->colliderSphereCenter.y, obj->colliderSphereCenter.z}},
                    {"colliderSphRadius",   obj->colliderSphereRadius},
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
            if (version < 1 || version > 12) return false;

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
                obj->pickable = o.value("pickable", true);
                if (o.contains("parentID")) obj->parentID = o["parentID"].get<int>();

                // version 7+: マテリアル色
                if (version >= 7 && o.contains("color")) {
                    obj->color = { o["color"][0], o["color"][1], o["color"][2], o["color"][3] };
                }
                objectManager_->ApplyObjectColor(*obj);

                // version 8+: UV スケール
                if (version >= 8 && o.contains("uvScale")) {
                    obj->uvScale = { o["uvScale"][0], o["uvScale"][1] };
                }
                // version 9+: タイル単位ハッシュランダム化の強度
                if (version >= 9 && o.contains("uvStochastic")) {
                    obj->uvStochastic = o["uvStochastic"].get<float>();
                }
                objectManager_->ApplyObjectUV(*obj);

                // version 12+: per-object 輪郭線フラグ（無ければ true=従来通り輪郭あり）
                obj->outlineEnabled = o.value("outlineEnabled", true);

                // version 10+: シーン内一意名 (TriggerAction のターゲット参照用)
                if (version >= 10 && o.contains("nameTag")) {
                    obj->nameTag = o["nameTag"].get<std::string>();
                }

                // version 11+: アンカーポイント (回転の旋回中心)
                if (version >= 11) {
                    obj->useAnchorPoint = o.value("useAnchorPoint", false);
                    if (o.contains("anchorPoint")) {
                        obj->anchorPoint = {
                            o["anchorPoint"][0].get<float>(),
                            o["anchorPoint"][1].get<float>(),
                            o["anchorPoint"][2].get<float>()
                        };
                    }
                }

                if (version >= 5) {
                    // version 5+: per-object コライダー設定を直接読む
                    obj->colliderTypeId = static_cast<CollisionTypeIdDef>(o.value("colliderTypeId", 0u));
                    if (o.contains("colliderAabbMin"))
                        obj->colliderAabbOffset.min = { o["colliderAabbMin"][0], o["colliderAabbMin"][1], o["colliderAabbMin"][2] };
                    if (o.contains("colliderAabbMax"))
                        obj->colliderAabbOffset.max = { o["colliderAabbMax"][0], o["colliderAabbMax"][1], o["colliderAabbMax"][2] };
                }
                if (version >= 6) {
                    obj->colliderShapeType = static_cast<ColliderShapeType>(o.value("colliderShapeType", 0u));
                    if (o.contains("colliderObbCenter"))
                        obj->colliderObbCenter = { o["colliderObbCenter"][0], o["colliderObbCenter"][1], o["colliderObbCenter"][2] };
                    if (o.contains("colliderObbSize"))
                        obj->colliderObbSize = { o["colliderObbSize"][0], o["colliderObbSize"][1], o["colliderObbSize"][2] };
                    if (o.contains("colliderObbEuler"))
                        obj->colliderObbEuler = { o["colliderObbEuler"][0], o["colliderObbEuler"][1], o["colliderObbEuler"][2] };
                    if (o.contains("colliderSphCenter"))
                        obj->colliderSphereCenter = { o["colliderSphCenter"][0], o["colliderSphCenter"][1], o["colliderSphCenter"][2] };
                    if (o.contains("colliderSphRadius"))
                        obj->colliderSphereRadius = o["colliderSphRadius"].get<float>();
                }
                if (version < 5) {
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
                // matWorld が確定したあとにコライダー内部 AABB を作り直す。
                // ApplyColliderTemplate は読み込みループ内で先に走るが、
                // その時点では UpdateMatrix 前なので matWorld_ が原点のままで
                // AABB が原点付近に張り付いてしまう (= NavGrid::Bake が障害物を
                // 認識せず敵が貫通する原因)。ここで再度 Update して位置を反映させる。
                if (obj->collider) {
                    obj->collider->Update();
                }
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
            j["version"] = 12;
            j["objects"] = json::array();

            for (const auto* obj : objects) {
                if (!obj) continue;

                j["objects"].push_back({
                    {"id",                  obj->id},
                    {"filePath",            obj->modelPath},
                    {"modelName",           obj->modelName},
                    {"nameTag",             obj->nameTag},
                    {"position",            {obj->position.x, obj->position.y, obj->position.z}},
                    {"rotate",              {obj->rotation.x, obj->rotation.y, obj->rotation.z}},
                    {"scale",               {obj->scale.x,    obj->scale.y,    obj->scale.z}},
                    {"useAnchorPoint",      obj->useAnchorPoint},
                    {"anchorPoint",         {obj->anchorPoint.x, obj->anchorPoint.y, obj->anchorPoint.z}},
                    {"color",               {obj->color.x,    obj->color.y,    obj->color.z,    obj->color.w}},
                    {"uvScale",             {obj->uvScale.x,  obj->uvScale.y}},
                    {"uvStochastic",        obj->uvStochastic},
                    {"outlineEnabled",      obj->outlineEnabled},
                    {"parentID",            obj->parentID},
                    {"isAnimation",         obj->isAnimation},
                    {"animationName",       obj->animationName},
                    {"pickable",            obj->pickable},
                    {"colliderEnabled",     obj->colliderEnabled},
                    {"colliderTypeId",      static_cast<uint32_t>(obj->colliderTypeId)},
                    {"colliderShapeType",   static_cast<uint32_t>(obj->colliderShapeType)},
                    {"colliderAabbMin",     {obj->colliderAabbOffset.min.x, obj->colliderAabbOffset.min.y, obj->colliderAabbOffset.min.z}},
                    {"colliderAabbMax",     {obj->colliderAabbOffset.max.x, obj->colliderAabbOffset.max.y, obj->colliderAabbOffset.max.z}},
                    {"colliderObbCenter",   {obj->colliderObbCenter.x,    obj->colliderObbCenter.y,    obj->colliderObbCenter.z}},
                    {"colliderObbSize",     {obj->colliderObbSize.x,      obj->colliderObbSize.y,      obj->colliderObbSize.z}},
                    {"colliderObbEuler",    {obj->colliderObbEuler.x,     obj->colliderObbEuler.y,     obj->colliderObbEuler.z}},
                    {"colliderSphCenter",   {obj->colliderSphereCenter.x, obj->colliderSphereCenter.y, obj->colliderSphereCenter.z}},
                    {"colliderSphRadius",   obj->colliderSphereRadius},
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
                obj->pickable = o.value("pickable", true);

                // version 7+: マテリアル色
                if (version >= 7 && o.contains("color")) {
                    obj->color = { o["color"][0], o["color"][1], o["color"][2], o["color"][3] };
                }
                objectManager_->ApplyObjectColor(*obj);

                // version 8+: UV スケール
                if (version >= 8 && o.contains("uvScale")) {
                    obj->uvScale = { o["uvScale"][0], o["uvScale"][1] };
                }
                // version 9+: タイル単位ハッシュランダム化の強度
                if (version >= 9 && o.contains("uvStochastic")) {
                    obj->uvStochastic = o["uvStochastic"].get<float>();
                }
                objectManager_->ApplyObjectUV(*obj);

                // version 12+: per-object 輪郭線フラグ（無ければ true=従来通り輪郭あり）
                obj->outlineEnabled = o.value("outlineEnabled", true);

                // version 10+: シーン内一意名
                if (version >= 10 && o.contains("nameTag")) {
                    obj->nameTag = o["nameTag"].get<std::string>();
                }

                // version 11+: アンカーポイント (回転の旋回中心)
                if (version >= 11) {
                    obj->useAnchorPoint = o.value("useAnchorPoint", false);
                    if (o.contains("anchorPoint")) {
                        obj->anchorPoint = {
                            o["anchorPoint"][0].get<float>(),
                            o["anchorPoint"][1].get<float>(),
                            o["anchorPoint"][2].get<float>()
                        };
                    }
                }

                if (version >= 5) {
                    obj->colliderTypeId = static_cast<CollisionTypeIdDef>(o.value("colliderTypeId", 0u));
                    if (o.contains("colliderAabbMin"))
                        obj->colliderAabbOffset.min = { o["colliderAabbMin"][0], o["colliderAabbMin"][1], o["colliderAabbMin"][2] };
                    if (o.contains("colliderAabbMax"))
                        obj->colliderAabbOffset.max = { o["colliderAabbMax"][0], o["colliderAabbMax"][1], o["colliderAabbMax"][2] };
                }
                if (version >= 6) {
                    obj->colliderShapeType = static_cast<ColliderShapeType>(o.value("colliderShapeType", 0u));
                    if (o.contains("colliderObbCenter"))
                        obj->colliderObbCenter = { o["colliderObbCenter"][0], o["colliderObbCenter"][1], o["colliderObbCenter"][2] };
                    if (o.contains("colliderObbSize"))
                        obj->colliderObbSize = { o["colliderObbSize"][0], o["colliderObbSize"][1], o["colliderObbSize"][2] };
                    if (o.contains("colliderObbEuler"))
                        obj->colliderObbEuler = { o["colliderObbEuler"][0], o["colliderObbEuler"][1], o["colliderObbEuler"][2] };
                    if (o.contains("colliderSphCenter"))
                        obj->colliderSphereCenter = { o["colliderSphCenter"][0], o["colliderSphCenter"][1], o["colliderSphCenter"][2] };
                    if (o.contains("colliderSphRadius"))
                        obj->colliderSphereRadius = o["colliderSphRadius"].get<float>();
                }
                if (version < 5) {
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
                // LoadScene と同じ理由でコライダー AABB を最新 matWorld で更新する。
                if (obj->collider) {
                    obj->collider->Update();
                }
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