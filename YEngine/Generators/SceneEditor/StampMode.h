#pragma once

#ifdef USE_IMGUI

// マップ上にオブジェクトを「クリックで連続配置」するモード。
//   入: Enter(sourceId) でスタンプ元 ID を渡して開始
//   表示: ゴースト (半透明の同モデル) がカーソル下に追従
//   配置: 左クリックで実体化、続けて配置可能
//   抜: Esc キー or 右クリック / Exit() 呼び出し

#include "Object3D/Object3d.h"
#include "Systems/Camera/Camera.h"
#include "Vector3.h"
#include "WorldTransform/WorldTransform.h"

#include <memory>

#include "imgui.h"

class ObjectManager;

namespace YoRigine {

class StampMode {
public:
  StampMode() = default;
  ~StampMode() = default;

  void SetObjectManager(ObjectManager *mgr) { objectManager_ = mgr; }
  void SetCamera(YoRigine::Camera *camera) { camera_ = camera; }

  bool IsActive() const { return active_; }

  // スタンプ元オブジェクトを指定してモード開始
  void Enter(int sourceObjectId);
  void Exit();

  // マウス位置からレイ → ヒット位置にゴーストを移動 / 左クリックで実体化 /
  // Esc・右クリックで終了
  void Update(const ImVec2 &viewportPos, const ImVec2 &viewportSize);

  // ゴーストの描画 (SceneEditor::Draw から呼ぶ)
  void DrawGhost();

private:
  // マウス座標 (viewport 相対 px) → ワールド空間レイ
  bool ScreenToWorldRay(float relX, float relY, float viewW, float viewH,
                        Vector3 &outOrigin, Vector3 &outDir) const;

  // ヒット位置を計算 (Raycast → なければ Y=0 平面交差にフォールバック)
  bool ComputePlacementPoint(const ImVec2 &viewportPos,
                             const ImVec2 &viewportSize, Vector3 &outPos) const;

  // スタンプ元のモデルでゴーストを構築
  void BuildGhost(int sourceObjectId);

  // 現在のヒット位置にソースの複製を実体化
  void PlaceAt(const Vector3 &pos);

private:
  ObjectManager *objectManager_ = nullptr;
  YoRigine::Camera *camera_ = nullptr;

  bool active_ = false;
  int sourceObjectId_ = -1;

  // 直近フレームのヒット位置 / 有効性
  Vector3 lastHitPos_ = {};
  bool lastHitValid_ = false;

  // ゴースト (転送オブジェクト)
  std::unique_ptr<YoRigine::Object3d> ghost_;
  std::unique_ptr<YoRigine::WorldTransform> ghostWT_;
};

} // namespace YoRigine

#endif // USE_IMGUI
