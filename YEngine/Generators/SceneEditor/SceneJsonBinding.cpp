#include "SceneJsonBinding.h"

namespace YoRigine::SceneJsonBinding {

//=============================================================================
// 配置オブジェクト
//
// ここに Add を 1 行足せば、保存・読み込み・（AutoJson::ShowImGui を使えば）
// デバッグ表示まで一度に対応できる。フィールドを増やしたら必ずここへ。
//=============================================================================
AutoJson BindPlacedObject(ObjectManager::PlacedObject &obj) {
  AutoJson binding;

  // ── 識別 ──
  binding.Add("id", &obj.id)
      .Add("filePath", &obj.modelPath)
      .Add("modelName", &obj.modelName)
      .Add("nameTag", &obj.nameTag)
      .Add("parentID", &obj.parentID);

  // ── トランスフォーム ──
  binding.Add("position", &obj.position)
      .Add("rotate", &obj.rotation)
      .Add("scale", &obj.scale)
      .Add("useAnchorPoint", &obj.useAnchorPoint)
      .Add("anchorPoint", &obj.anchorPoint);

  // ── 描画 ──
  binding.Add("color", &obj.color)
      .Add("uvScale", &obj.uvScale)
      .Add("uvStochastic", &obj.uvStochastic)
      .Add("outlineEnabled", &obj.outlineEnabled)
      .Add("castShadow", &obj.castShadow)
      .Add("visible", &obj.visible)
      .Add("pickable", &obj.pickable);

  // ── アニメーション ──
  binding.Add("isAnimation", &obj.isAnimation)
      .Add("animationName", &obj.animationName);

  // ── コライダー ──
  binding.Add("colliderEnabled", &obj.colliderEnabled)
      .Add("colliderCameraFade", &obj.colliderCameraFade)
      .Add("colliderTypeId", &obj.colliderTypeId)
      .Add("colliderShapeType", &obj.colliderShapeType)
      .Add("colliderAabbMin", &obj.colliderAabbOffset.min)
      .Add("colliderAabbMax", &obj.colliderAabbOffset.max)
      .Add("colliderObbCenter", &obj.colliderObbCenter)
      .Add("colliderObbSize", &obj.colliderObbSize)
      .Add("colliderObbEuler", &obj.colliderObbEuler)
      .Add("colliderSphCenter", &obj.colliderSphereCenter)
      .Add("colliderSphRadius", &obj.colliderSphereRadius);

  return binding;
}

//=============================================================================
// マテリアル上書き (メッシュ 1 スロットぶん)
//=============================================================================
AutoJson BindMaterialOverride(MeshMaterialOverride &slot) {
  AutoJson binding;

  binding.Add("overrideBaseColor", &slot.overrideBaseColor)
      .Add("baseColor", &slot.baseColor)
      .Add("texturePath", &slot.texturePath);

  return binding;
}

//=============================================================================
// シーン設定
//
// オブジェクトとは別に保存し、シーン切り替え時に前シーンの値を引き継がない。
//=============================================================================
AutoJson BindSceneSettings(SceneViewSettings &settings,
                           bool &collisionFrustumCulling) {
  AutoJson binding;

  binding.Add("drawFrustumCulling", &settings.enableDrawFrustumCulling)
      .Add("collisionFrustumCulling", &collisionFrustumCulling)
      .Add("showCollider", &settings.showCollider)
      .Add("showColliderSelectedOnly", &settings.showColliderSelectedOnly)
      .Add("showSelectionOutline", &settings.showSelectionOutline);

  return binding;
}

} // namespace YoRigine::SceneJsonBinding
