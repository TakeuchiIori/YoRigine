#include "SceneSerializer.h"

// Engine
#include <Collision/Core/CollisionTypeIdDef.h>
#include <Debugger/Logger.h>

// C++
#include <unordered_map>

using json = nlohmann::json;

namespace YoRigine {

//=============================================================================
// 旧形式 (version <= 14) の読み込み。
//
// 旧形式ではベクトルが [x, y, z] の配列で保存されていた。現在の AutoJson
// 形式は {"x":.., "y":.., "z":..} のオブジェクトなので、そのままでは
// 読めない。既存のシーン JSON を壊さないための読み込み専用パスがこれで、
// 一度読み込んで保存し直せば新形式へ移行し、以後この関数は通らない。
//
// 新機能をここへ足さないこと。追加項目は SceneJsonBinding にだけ書く。
//=============================================================================

namespace {

Vector3 ReadVec3(const json &source, const char *key, const Vector3 &fallback) {
  if (!source.contains(key) || source[key].size() < 3) {
    return fallback;
  }
  return {source[key][0].get<float>(), source[key][1].get<float>(),
          source[key][2].get<float>()};
}

Vector4 ReadVec4(const json &source, const char *key, const Vector4 &fallback) {
  if (!source.contains(key) || source[key].size() < 4) {
    return fallback;
  }
  return {source[key][0].get<float>(), source[key][1].get<float>(),
          source[key][2].get<float>(), source[key][3].get<float>()};
}

Vector2 ReadVec2(const json &source, const char *key, const Vector2 &fallback) {
  if (!source.contains(key) || source[key].size() < 2) {
    return fallback;
  }
  return {source[key][0].get<float>(), source[key][1].get<float>()};
}

// version 1-4 のモデル名単位のコライダーテンプレート
struct LegacyColliderTemplate {
  CollisionTypeIdDef typeId = CollisionTypeIdDef::kNone;
  AABB aabb{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
};

std::unordered_map<std::string, LegacyColliderTemplate>
ReadLegacyTemplates(const json &root, int version) {
  std::unordered_map<std::string, LegacyColliderTemplate> templates;
  if (version > 4 || !root.contains("colliderTemplates")) {
    return templates;
  }

  for (const auto &[modelName, templateJson] :
       root["colliderTemplates"].items()) {
    LegacyColliderTemplate entry;
    entry.typeId =
        static_cast<CollisionTypeIdDef>(templateJson.value("typeId", 0u));
    entry.aabb.min = ReadVec3(templateJson, "aabbMin", entry.aabb.min);
    entry.aabb.max = ReadVec3(templateJson, "aabbMax", entry.aabb.max);
    templates[modelName] = entry;
  }
  return templates;
}

} // namespace

bool SceneSerializer::LoadLegacyObjects(const json &root, int version) {
  if (!objectManager_ || !root.contains("objects")) {
    return false;
  }

  const auto legacyTemplates = ReadLegacyTemplates(root, version);
  std::unordered_map<int, int> oldToNewId;

  for (const auto &source : root["objects"]) {
    auto *obj = objectManager_->CreateObject(
        source.value("filePath", std::string{}),
        source.value("isAnimation", false),
        source.value("animationName", std::string{}));
    if (!obj) {
      continue;
    }

    oldToNewId[source.value("id", -1)] = obj->id;

    // ── トランスフォーム ──
    obj->position = ReadVec3(source, "position", obj->position);
    obj->rotation = ReadVec3(source, "rotate", obj->rotation);
    obj->scale = ReadVec3(source, "scale", obj->scale);

    // version 11+: アンカーポイント (回転の旋回中心)
    if (version >= 11) {
      obj->useAnchorPoint = source.value("useAnchorPoint", false);
      obj->anchorPoint = ReadVec3(source, "anchorPoint", obj->anchorPoint);
    }

    // ── 基本フラグ ──
    obj->parentID = source.value("parentID", -1);
    obj->pickable = source.value("pickable", true);
    obj->colliderEnabled = source.value("colliderEnabled", false);
    obj->colliderCameraFade = source.value("colliderCameraFade", false);

    // version 10+: シーン内一意名 (TriggerAction のターゲット参照用)
    if (version >= 10) {
      obj->nameTag = source.value("nameTag", std::string{});
    }
    // version 12+: 輪郭線 / version 13+: 影キャスト (無ければ従来どおり有効)
    obj->outlineEnabled = source.value("outlineEnabled", true);
    obj->castShadow = source.value("castShadow", true);

    // ── マテリアル / UV ──
    if (version >= 7) {
      obj->color = ReadVec4(source, "color", obj->color);
    }
    objectManager_->ApplyObjectColor(*obj);

    if (version >= 8) {
      obj->uvScale = ReadVec2(source, "uvScale", obj->uvScale);
    }
    if (version >= 9) {
      obj->uvStochastic = source.value("uvStochastic", 0.0f);
    }
    objectManager_->ApplyObjectUV(*obj);

    // ── コライダー ──
    if (version >= 5) {
      obj->colliderTypeId =
          static_cast<CollisionTypeIdDef>(source.value("colliderTypeId", 0u));
      obj->colliderAabbOffset.min =
          ReadVec3(source, "colliderAabbMin", obj->colliderAabbOffset.min);
      obj->colliderAabbOffset.max =
          ReadVec3(source, "colliderAabbMax", obj->colliderAabbOffset.max);
    } else {
      // version 1-4: モデル名単位のテンプレートを個別オブジェクトへ展開する
      const auto it = legacyTemplates.find(obj->modelName);
      if (it != legacyTemplates.end()) {
        obj->colliderTypeId = it->second.typeId;
        obj->colliderAabbOffset = it->second.aabb;
      }
      // version 3 は per-object AABB が別キーで入っている場合がありそちらを優先
      if (version == 3) {
        obj->colliderAabbOffset.min =
            ReadVec3(source, "aabbMin", obj->colliderAabbOffset.min);
        obj->colliderAabbOffset.max =
            ReadVec3(source, "aabbMax", obj->colliderAabbOffset.max);
      }
    }

    if (version >= 6) {
      obj->colliderShapeType =
          static_cast<ColliderShapeType>(source.value("colliderShapeType", 0u));
      obj->colliderObbCenter =
          ReadVec3(source, "colliderObbCenter", obj->colliderObbCenter);
      obj->colliderObbSize =
          ReadVec3(source, "colliderObbSize", obj->colliderObbSize);
      obj->colliderObbEuler =
          ReadVec3(source, "colliderObbEuler", obj->colliderObbEuler);
      obj->colliderSphereCenter =
          ReadVec3(source, "colliderSphCenter", obj->colliderSphereCenter);
      obj->colliderSphereRadius =
          source.value("colliderSphRadius", obj->colliderSphereRadius);
    }

    objectManager_->ApplyColliderTemplate(*obj);
  }

  ResolveHierarchy(oldToNewId, /*remapThroughSetParent=*/false);
  return true;
}

} // namespace YoRigine
