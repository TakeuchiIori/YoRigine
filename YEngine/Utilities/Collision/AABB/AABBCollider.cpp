#include "AABBCollider.h"

void AABBCollider::InitJson(YoRigine::JsonManager *jsonManager) {
  // 衝突球のオフセットや半径を JSON に登録
  jsonManager->SetCategory("Colliders");
  jsonManager->Register("Collider Offset Min X", &aabbOffset_.min.x);
  jsonManager->Register("Collider Offset Min Y", &aabbOffset_.min.y);
  jsonManager->Register("Collider Offset Min Z", &aabbOffset_.min.z);
  jsonManager->Register("Collider Offset Max X", &aabbOffset_.max.x);
  jsonManager->Register("Collider Offset Max Y", &aabbOffset_.max.y);
  jsonManager->Register("Collider Offset Max Z", &aabbOffset_.max.z);
}

Vector3 AABBCollider::GetCenterPosition() const {
  Vector3 newPos;
  newPos.x = wt_->matWorld_.m[3][0];
  newPos.y = wt_->matWorld_.m[3][1];
  newPos.z = wt_->matWorld_.m[3][2];
  return newPos;
}

const YoRigine::WorldTransform &AABBCollider::GetWorldTransform() { return *wt_; }

Vector3 AABBCollider::GetEulerRotation() const {
  return wt_ ? wt_->rotate_ : Vector3{};
}

void AABBCollider::Initialize() {
  shape_ = ColliderShape::AABB;
  BaseCollider::Initialize();

  aabb_.min = {0.0f, 0.0f, 0.0f};
  aabb_.max = {0.0f, 0.0f, 0.0f};

  aabbOffset_.min = {-1.0f, -1.0f, -1.0f};
  aabbOffset_.max = {1.0f, 1.0f, 1.0f};
}

// ✅ 修正後：offset をローカル空間のバウンドとして直接適用
void AABBCollider::Update() {
  const Vector3 scale = GetWorldTransform().scale_;
  const Vector3 center = GetCenterPosition();

  // aabbOffset_ はローカル空間の min/max。スケールを乗じてワールド空間へ変換
  const Vector3 localMin = {
      aabbOffset_.min.x * scale.x,
      aabbOffset_.min.y * scale.y,
      aabbOffset_.min.z * scale.z,
  };
  const Vector3 localMax = {
      aabbOffset_.max.x * scale.x,
      aabbOffset_.max.y * scale.y,
      aabbOffset_.max.z * scale.z,
  };

  // center にオフセットを加算（負スケール対応で min/max を正規化）
  aabb_.min = {
      center.x + (std::min)(localMin.x, localMax.x),
      center.y + (std::min)(localMin.y, localMax.y),
      center.z + (std::min)(localMin.z, localMax.z),
  };
  aabb_.max = {
      center.x + (std::max)(localMin.x, localMax.x),
      center.y + (std::max)(localMin.y, localMax.y),
      center.z + (std::max)(localMin.z, localMax.z),
  };
}

void AABBCollider::Draw() {
  line_->Reset();
  line_->DrawAABB(aabb_.min, aabb_.max);
  line_->DrawLine();
}
