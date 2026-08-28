#include "SceneObjectRenderer.h"

// Engine
#include "../ObjectSelector.h"
#include "../PickBuffer.h"
#include "Model.h"
#include <Collision/Core/CollisionManager.h>
#include <Drawer/InstancedObject3d.h>
#include <Motion/Editor/MotionEditor.h>
#include <Systems/Camera/Camera.h>

#ifdef USE_IMGUI
#include <DirectX/DirectXCommon.h>
#endif

// Math
#include "Frustum/Frustum.h"
#include "MathFunc.h"

#include <cmath>

namespace YoRigine {

//=============================================================================
// 視錐台カリング用の外接ボックス
//=============================================================================
AABB SceneObjectRenderer::ComputeDrawBounds(
    const ObjectManager::PlacedObject &obj) const {
  // コライダー有効時はそのワールド AABB を流用する
  if (obj.collider && obj.colliderEnabled) {
    return CollisionManager::ComputeWorldAABB(obj.collider.get());
  }

  // 無いときは position ± (scale * 係数) で大雑把に見積もる
  const float factor = context_.viewSettings->drawBoundsScaleFactor;
  const float ex = std::fabs(obj.scale.x) * factor;
  const float ey = std::fabs(obj.scale.y) * factor;
  const float ez = std::fabs(obj.scale.z) * factor;
  return AABB{{obj.position.x - ex, obj.position.y - ey, obj.position.z - ez},
              {obj.position.x + ex, obj.position.y + ey, obj.position.z + ez}};
}

//=============================================================================
// カラーパス
//=============================================================================
void SceneObjectRenderer::Draw() {
  if (!context_.IsValid() || !context_.camera) {
    return;
  }

  // 視錐台の抽出 (有効時のみ)
  Frustum frustum{};
  const bool useCulling = context_.viewSettings->enableDrawFrustumCulling;
  if (useCulling) {
    frustum = FrustumUtil::ExtractFromViewProjection(
        context_.camera->GetViewProjectionMatrix());
  }

  // インスタンシング: 非アニメオブジェクトをモデル単位でまとめて描画
  auto *instRenderer = ::InstancedObject3d::GetInstance();
  instRenderer->Begin(context_.camera);
  const Matrix4x4 &vp = context_.camera->GetViewProjectionMatrix();

  MotionEditor *motionEditor = context_.motionEditor;

  for (auto *obj : context_.objectManager->GetAllActiveObjects()) {
    if (!obj || !obj->object || !obj->worldTransform || !obj->visible) {
      continue;
    }

    if (useCulling &&
        !FrustumUtil::IsAABBVisible(frustum, ComputeDrawBounds(*obj))) {
      continue;
    }

    // ボーン表示中の対象は MotionEditor 側が描くのでスキップ
    if (motionEditor && motionEditor->IsDrawBone() &&
        obj->id == motionEditor->GetTargetObjectId()) {
      continue;
    }

    Model *model = obj->object->GetModel();
    const bool canInstance =
        (model && !obj->isAnimation && !model->GetHasBones());

    // カメラ遮蔽フェードは alpha blend ではなく深度書き込み ON の
    // ディザーフェードを使う。複雑な建物の内部面が描画順で透けて
    // まだらになるのを防ぐ。
    const bool isCameraDitherFade =
        canInstance && obj->colliderCameraFade && obj->color.w < 0.99f;

    if (isCameraDitherFade) {
      instRenderer->SubmitDitherFade(*obj);
    } else if (canInstance) {
      instRenderer->Submit(*obj);
    } else {
      // アニメ付き / 特殊エフェクト: 従来の個別 Draw 経路
      obj->object->SetMaterialColor(obj->color);
      obj->object->Draw(context_.camera, *obj->worldTransform);
      continue;
    }

    // PickBuffer や他システムが worldTransform の CB を参照するため、
    // Object3d::Draw を経由しなくても CB を最新行列で同期する
    const Matrix4x4 &world = obj->worldTransform->GetMatWorld();
    obj->worldTransform->SetMapWVP(world * vp);
    obj->worldTransform->SetMapWorld(world);
  }

  // インスタンス分を1ドローコール/モデルでまとめて描画
  instRenderer->DrawAll(context_.camera);
}

//=============================================================================
// シャドウマップパス
//=============================================================================
void SceneObjectRenderer::DrawShadow() {
  if (!context_.IsValid()) {
    return;
  }

  auto *instRenderer = ::InstancedObject3d::GetInstance();
  // camera は null 可 (影パスは WVP 不使用)
  instRenderer->Begin(context_.camera);

  for (auto *obj : context_.objectManager->GetAllActiveObjects()) {
    if (!obj || !obj->object || !obj->worldTransform || !obj->visible) {
      continue;
    }
    // 「影を落とす」が OFF のオブジェクトはシャドウマップへ描かない。
    // 巨大スケールの地面などがシャドウマップを埋めて影がチラつくのを防ぐ。
    if (!obj->castShadow) {
      continue;
    }

    Model *model = obj->object->GetModel();
    const bool canInstance =
        (model && !obj->isAnimation && !model->GetHasBones());

    if (canInstance) {
      instRenderer->Submit(*obj);
    } else {
      obj->object->DrawShadow(*obj->worldTransform);
    }
  }

  instRenderer->DrawShadow();
}

//=============================================================================
// ピックパス
//
// DrawShadow と同じ頂点/インデックスバッファを流用し、
// PickPSO で ObjectID を R32_UINT の RT へ書き込む。
//=============================================================================
void SceneObjectRenderer::DrawForPick() {
#ifdef USE_IMGUI
  if (!context_.IsValid() || !context_.camera) {
    return;
  }

  auto *cmd = DirectXCommon::GetInstance()->GetCommandList().Get();

  for (auto *obj : context_.objectManager->GetAllActiveObjects()) {
    if (!obj || !obj->object || !obj->worldTransform || !obj->visible) {
      continue;
    }
    // pickable=false の背景オブジェクト (地面など) は Pick バッファに描かない。
    // クリックは裏の手前オブジェクトか空 (-1) に抜ける。
    if (!obj->pickable) {
      continue;
    }

    auto *model = obj->object->GetModel();
    if (!model) {
      continue;
    }

    // WVP 行列 CBV (b0)
    cmd->SetGraphicsRootConstantBufferView(
        PickBuffer::ROOT_PARAM_MVP_CBV,
        obj->worldTransform->GetConstBuffer()->GetGPUVirtualAddress());

    // ObjectID ルート定数 (b1)  ※ 0 は空選択予約なので +1
    const uint32_t encodedID = static_cast<uint32_t>(obj->id) + 1;
    cmd->SetGraphicsRoot32BitConstant(PickBuffer::ROOT_PARAM_OBJECT_ID,
                                      encodedID, 0);

    for (auto &mesh : model->GetMeshes()) {
      if (mesh->HasBones() && model->GetSkinCluster()) {
        mesh->RecordDrawCommands(cmd, *model->GetSkinCluster());
      } else {
        mesh->RecordDrawCommands(cmd);
      }
      cmd->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);
    }
  }
#endif
}

} // namespace YoRigine
