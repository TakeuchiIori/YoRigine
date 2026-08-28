#include "ScenePlacementService.h"

// Engine
#include "../ObjectSelector.h"
#include <Collision/Core/CollisionManager.h>
#include <Debugger/Logger.h>
#include <Ray/Raycast.h>

// C++
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace YoRigine {

namespace {
constexpr float kPi = 3.14159265358979f;

// 真下 Raycast の開始高さと最大距離。
// 高さ 100 から 500 ぶん撃てば、通常のステージスケールでは必ず地面に届く。
constexpr float kSnapRayStartHeight = 100.0f;
constexpr float kSnapRayMaxDistance = 500.0f;

float SnapValue(float value, float step) {
  if (step <= 0.0f) {
    return value;
  }
  return std::round(value / step) * step;
}
} // namespace

//=============================================================================
// モデルファイルを配置
//=============================================================================
ObjectManager::PlacedObject *
ScenePlacementService::PlaceModel(const std::string &modelPath) {
  if (!context_.IsValid()) {
    return nullptr;
  }

  try {
    if (modelPath.empty() || !std::filesystem::exists(modelPath)) {
      Logger("[ScenePlacement] パスが不正です: " + modelPath);
      return nullptr;
    }

    const std::filesystem::path full(modelPath);
    const std::filesystem::path relative =
        std::filesystem::relative(full, modelFolderPath_);

    // glTF/glb はアニメーション付きの可能性があるのでアニメ経路で読み込む
    std::string ext = full.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    const bool isAnimated = (ext == ".gltf" || ext == ".glb");

    auto *obj =
        context_.objectManager->CreateObject(relative.string(), isAnimated);
    if (!obj) {
      return nullptr;
    }

    if (context_.selector) {
      context_.selector->ClearSelection();
      context_.selector->AddToSelection(obj->id);
    }
    context_.objectManager->UpdateObjectTransform(*obj);
    Logger("[ScenePlacement] 配置しました: " + full.filename().string());
    return obj;
  } catch (const std::exception &e) {
    Logger(std::string("[ScenePlacement] 配置に失敗しました: ") + e.what());
    return nullptr;
  }
}

//=============================================================================
// 地面への吸着
//=============================================================================
int ScenePlacementService::SnapSelectionToSurface() {
  if (!context_.IsValid() || !context_.selector) {
    return 0;
  }
  auto *collisionManager = CollisionManager::GetInstance();
  if (!collisionManager) {
    return 0;
  }

  int snapped = 0;
  for (const int id : context_.selector->GetSelectedIds()) {
    auto *obj = context_.objectManager->GetObjectById(id);
    if (!obj) {
      continue;
    }

    // 自分自身に当たらないよう、対象のコライダーを一時的に無効化する
    const bool wasEnabled =
        obj->collider && obj->collider->IsCollisionEnabled();
    if (wasEnabled) {
      obj->collider->SetCollisionEnabled(false);
    }

    Ray ray{};
    ray.origin = {obj->position.x, obj->position.y + kSnapRayStartHeight,
                  obj->position.z};
    ray.direction = {0.0f, -1.0f, 0.0f};

    RaycastHit hit;
    if (collisionManager->Raycast(ray, kSnapRayMaxDistance, &hit, {})) {
      obj->position = hit.hitPoint;
      context_.objectManager->UpdateObjectTransform(*obj);
      ++snapped;
    }

    if (wasEnabled) {
      obj->collider->SetCollisionEnabled(true);
    }
  }
  return snapped;
}

//=============================================================================
// グリッドへの整列
//=============================================================================
int ScenePlacementService::SnapSelectionToGrid(float gridSize) {
  if (!context_.IsValid() || !context_.selector || gridSize <= 0.0f) {
    return 0;
  }

  int snapped = 0;
  for (const int id : context_.selector->GetSelectedIds()) {
    auto *obj = context_.objectManager->GetObjectById(id);
    if (!obj) {
      continue;
    }
    obj->position = {SnapValue(obj->position.x, gridSize),
                     SnapValue(obj->position.y, gridSize),
                     SnapValue(obj->position.z, gridSize)};
    context_.objectManager->UpdateObjectTransform(*obj);
    ++snapped;
  }
  return snapped;
}

//=============================================================================
// 回転の整列
//=============================================================================
int ScenePlacementService::SnapSelectionRotation(float stepDegrees) {
  if (!context_.IsValid() || !context_.selector || stepDegrees <= 0.0f) {
    return 0;
  }

  const float step = stepDegrees * (kPi / 180.0f);

  int snapped = 0;
  for (const int id : context_.selector->GetSelectedIds()) {
    auto *obj = context_.objectManager->GetObjectById(id);
    if (!obj) {
      continue;
    }
    obj->rotation = {SnapValue(obj->rotation.x, step),
                     SnapValue(obj->rotation.y, step),
                     SnapValue(obj->rotation.z, step)};
    context_.objectManager->UpdateObjectTransform(*obj);
    ++snapped;
  }
  return snapped;
}

} // namespace YoRigine
