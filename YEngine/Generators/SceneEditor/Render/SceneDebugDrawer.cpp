#include "SceneDebugDrawer.h"

// Engine
#include "../ObjectSelector.h"
#include <Collision/Core/CollisionManager.h>
#include <Collision/Core/CollisionTypeIdDef.h>
#include <Collision/OBB/OBBCollider.h>
#include <Collision/Sphere/SphereCollider.h>
#include <Systems/Camera/Camera.h>

// Math
#include "MathFunc.h"

#include <algorithm>
#include <cfloat>

namespace YoRigine {

namespace {

// 選択ハイライトの色 (Blender 風オレンジ)
const Vector4 kSelectionColor{1.0f, 0.55f, 0.0f, 1.0f};

// コライダーのタイプ ID ごとの色分け。
// 種類が増えたときは default のグレーに落ちるだけなので追記は任意。
Vector4 ColorForColliderType(uint32_t typeKey) {
  switch (static_cast<CollisionTypeIdDef>(typeKey)) {
  case CollisionTypeIdDef::kStaticWall:
    return {1.0f, 0.2f, 0.2f, 1.0f}; // 赤
  case CollisionTypeIdDef::kNavObstacle:
    return {1.0f, 0.8f, 0.0f, 1.0f}; // 黄
  case CollisionTypeIdDef::kNavTrigger:
    return {0.2f, 0.5f, 1.0f, 1.0f}; // 青
  case CollisionTypeIdDef::kWaypoint:
    return {0.2f, 1.0f, 0.3f, 1.0f}; // 緑
  case CollisionTypeIdDef::kEventTrigger:
    return {0.9f, 0.3f, 0.9f, 1.0f}; // マゼンタ
  default:
    return {0.6f, 0.6f, 0.6f, 1.0f}; // グレー
  }
}

} // namespace

void SceneDebugDrawer::Initialize() {
  colliderCubes_.Initialize();
  colliderSpheres_.Initialize();
  colliderLineCapsule_.Initialize();
}

void SceneDebugDrawer::SetCamera(Camera *camera) {
  colliderCubes_.SetCamera(camera);
  colliderSpheres_.SetCamera(camera);
  colliderLineCapsule_.SetCamera(camera);
}

//=============================================================================
// 選択中ハイライト
//
// 色のマテリアル上書きを避け、モデル外接の世界 AABB をオレンジ枠で表示する。
// コライダーデバッグ表示の ON/OFF に関係なく描く。
//=============================================================================
void SceneDebugDrawer::DrawSelectionOutline() {
  ObjectSelector *selector = context_.selector;
  if (!selector || !selector->HasSelection()) {
    return;
  }

  colliderCubes_.Begin();
  for (auto *obj : context_.objectManager->GetAllActiveObjects()) {
    if (!obj || !obj->object || !obj->worldTransform) {
      continue;
    }
    if (!selector->IsSelected(obj->id)) {
      continue;
    }

    AABB local{};
    if (!context_.objectManager->ComputeModelLocalAABB(*obj, local)) {
      continue;
    }

    // ローカル AABB の 8 隅をワールド変換して、その外接 AABB を求める
    const Vector3 corners[8] = {
        {local.min.x, local.min.y, local.min.z},
        {local.max.x, local.min.y, local.min.z},
        {local.min.x, local.max.y, local.min.z},
        {local.max.x, local.max.y, local.min.z},
        {local.min.x, local.min.y, local.max.z},
        {local.max.x, local.min.y, local.max.z},
        {local.min.x, local.max.y, local.max.z},
        {local.max.x, local.max.y, local.max.z},
    };
    const Matrix4x4 &mw = obj->worldTransform->GetMatWorld();
    Vector3 worldMin = {FLT_MAX, FLT_MAX, FLT_MAX};
    Vector3 worldMax = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (const auto &corner : corners) {
      const Vector3 wp = Transform(corner, mw);
      worldMin.x = std::min(worldMin.x, wp.x);
      worldMin.y = std::min(worldMin.y, wp.y);
      worldMin.z = std::min(worldMin.z, wp.z);
      worldMax.x = std::max(worldMax.x, wp.x);
      worldMax.y = std::max(worldMax.y, wp.y);
      worldMax.z = std::max(worldMax.z, wp.z);
    }
    colliderCubes_.AddAABB(worldMin, worldMax, kSelectionColor);
  }
  colliderCubes_.Flush();
}

//=============================================================================
// コライダー形状
//=============================================================================
void SceneDebugDrawer::DrawColliders() {
  const SceneViewSettings &settings = *context_.viewSettings;
  ObjectSelector *selector = context_.selector;

  colliderCubes_.Begin();
  colliderSpheres_.Begin();
  colliderLineCapsule_.SetColor({0.6f, 0.6f, 0.6f, 1.0f});

  for (auto *obj : context_.objectManager->GetAllActiveObjects()) {
    if (!obj || !obj->collider || !obj->colliderEnabled) {
      continue;
    }
    if (settings.showColliderSelectedOnly &&
        (!selector || !selector->IsSelected(obj->id))) {
      continue;
    }

    const Vector4 color = ColorForColliderType(obj->collider->GetTypeID());

    if (auto *aabb = dynamic_cast<AABBCollider *>(obj->collider.get())) {
      colliderCubes_.AddAABB(aabb->GetAABB().min, aabb->GetAABB().max, color);
    } else if (auto *obb = dynamic_cast<OBBCollider *>(obj->collider.get())) {
      colliderCubes_.AddOBB(obb->GetOBB().center, obb->GetOBB().rotation,
                            obb->GetOBB().size, color);
    } else if (auto *sphere =
                   dynamic_cast<SphereCollider *>(obj->collider.get())) {
      colliderSpheres_.AddSphere(sphere->GetSphere().center,
                                 sphere->GetSphere().radius, color);
    } else if (auto *capsule =
                   dynamic_cast<CapsuleCollider *>(obj->collider.get())) {
      // Capsule は単位形状化が複雑 (start/end が可変) なので Line で描く
      const auto &c = capsule->GetCapsule();
      colliderLineCapsule_.DrawCapsule(c.start, c.end, c.radius, 12);
    }
  }

  // 1 DrawInstanced (Cube), 1 DrawInstanced (Sphere), 1 DrawCall (Capsule Line)
  colliderCubes_.Flush();
  colliderSpheres_.Flush();
  colliderLineCapsule_.DrawLine();
}

//=============================================================================
// BroadPhase グリッド (カメラ周辺のみ)
//=============================================================================
void SceneDebugDrawer::DrawBroadPhaseGrid() {
  if (!context_.camera) {
    return;
  }

  colliderCubes_.Begin();
  const Vector3 camPos = context_.camera->GetTranslate();
  CollisionManager::GetInstance()->GetBroadPhaseGrid().DrawDebugAroundCamera(
      &colliderCubes_, camPos, context_.viewSettings->broadPhaseGridDrawRadius,
      Vector4{0.3f, 0.8f, 0.3f, 0.4f});
  colliderCubes_.Flush();
}

//=============================================================================
// まとめ
//=============================================================================
void SceneDebugDrawer::Draw() {
  if (!context_.IsValid()) {
    return;
  }

  const SceneViewSettings &settings = *context_.viewSettings;

  if (settings.showSelectionOutline) {
    DrawSelectionOutline();
  }
  if (!settings.showCollider) {
    return;
  }

  DrawColliders();

  if (settings.showBroadPhaseGrid) {
    DrawBroadPhaseGrid();
  }
}

} // namespace YoRigine
