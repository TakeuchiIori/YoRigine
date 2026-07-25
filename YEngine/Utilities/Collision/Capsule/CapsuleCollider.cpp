#include "CapsuleCollider.h"
#include "Matrix4x4.h"

void CapsuleCollider::InitJson(YoRigine::JsonManager *jsonManager) {
  jsonManager->SetCategory("Colliders");
  jsonManager->Register("Capsule Offset X", &centerOffset_.x);
  jsonManager->Register("Capsule Offset Y", &centerOffset_.y);
  jsonManager->Register("Capsule Offset Z", &centerOffset_.z);
  jsonManager->Register("Capsule Radius", &radius_);
  jsonManager->Register("Capsule HalfHeight", &halfHeight_);
  jsonManager->Register("Capsule AxisLocal X", &axisLocal_.x);
  jsonManager->Register("Capsule AxisLocal Y", &axisLocal_.y);
  jsonManager->Register("Capsule AxisLocal Z", &axisLocal_.z);
}

Vector3 CapsuleCollider::GetCenterPosition() const {
  Vector3 worldPos = {0, 0, 0};
  if (wt_) {
    worldPos.x = wt_->matWorld_.m[3][0];
    worldPos.y = wt_->matWorld_.m[3][1];
    worldPos.z = wt_->matWorld_.m[3][2];
  }
  return worldPos + centerOffset_;
}

const YoRigine::WorldTransform &CapsuleCollider::GetWorldTransform() { return *wt_; }

Vector3 CapsuleCollider::GetEulerRotation() const {
  return wt_ ? wt_->rotate_ : Vector3{};
}

void CapsuleCollider::Initialize() {
  shape_ = ColliderShape::Capsule;
  BaseCollider::Initialize();
  halfHeight_ = 1.0f;
  radius_ = 0.5f;
  centerOffset_ = {0, 0, 0};
  axisLocal_ = {0, 1, 0};
}

void CapsuleCollider::Update() {
  if (!wt_)
    return;

  // ワールド行列から、軸方向(局所→ワールド)とセンター位置を取得
  const Matrix4x4 &world = wt_->matWorld_;

  // 純粋な回転行列を取り出す (スケール除去)。OBBCollider と同じ流儀。
  Vector3 worldScale = {
      Length(Vector3(world.m[0][0], world.m[0][1], world.m[0][2])),
      Length(Vector3(world.m[1][0], world.m[1][1], world.m[1][2])),
      Length(Vector3(world.m[2][0], world.m[2][1], world.m[2][2]))};

  Matrix4x4 rotOnly{};
  rotOnly.m[3][3] = 1.0f;
  if (worldScale.x > 0.0f) {
    rotOnly.m[0][0] = world.m[0][0] / worldScale.x;
    rotOnly.m[0][1] = world.m[0][1] / worldScale.x;
    rotOnly.m[0][2] = world.m[0][2] / worldScale.x;
  }
  if (worldScale.y > 0.0f) {
    rotOnly.m[1][0] = world.m[1][0] / worldScale.y;
    rotOnly.m[1][1] = world.m[1][1] / worldScale.y;
    rotOnly.m[1][2] = world.m[1][2] / worldScale.y;
  }
  if (worldScale.z > 0.0f) {
    rotOnly.m[2][0] = world.m[2][0] / worldScale.z;
    rotOnly.m[2][1] = world.m[2][1] / worldScale.z;
    rotOnly.m[2][2] = world.m[2][2] / worldScale.z;
  }

  // axisLocal_ をワールドへ変換し、半長分の両端を計算
  Vector3 axisWorld = Transform(axisLocal_, rotOnly);
  float axisLen = Length(axisWorld);
  if (axisLen > 1e-6f) {
    axisWorld = axisWorld * (1.0f / axisLen);
  } else {
    axisWorld = {0, 1, 0};
  }

  Vector3 center = GetCenterPosition();
  // スケールは Y 軸方向に追従させる (人型を伸ばす想定)
  float effectiveHalf = halfHeight_ * std::fabs(worldScale.y);
  float effectiveRadius =
      radius_ * std::fabs(0.5f * (worldScale.x + worldScale.z));

  capsule_.start = center - axisWorld * effectiveHalf;
  capsule_.end = center + axisWorld * effectiveHalf;
  capsule_.radius = effectiveRadius;
}

void CapsuleCollider::Draw() {
  if (!line_)
    return;
  line_->Reset();
  line_->DrawCapsule(capsule_.start, capsule_.end, capsule_.radius, 16);
  line_->DrawLine();
}
