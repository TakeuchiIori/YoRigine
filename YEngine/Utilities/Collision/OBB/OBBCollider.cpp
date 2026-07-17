#include "OBBCollider.h"
#include "Matrix4x4.h"

void OBBCollider::InitJson(YoRigine::JsonManager *jsonManager) {
  jsonManager->SetCategory("Colliders");

  jsonManager->Register("OBB Offset Center X", &obbOffset_.center.x);
  jsonManager->Register("OBB Offset Center Y", &obbOffset_.center.y);
  jsonManager->Register("OBB Offset Center Z", &obbOffset_.center.z);

  jsonManager->Register("OBB Offset Size X", &obbOffset_.size.x);
  jsonManager->Register("OBB Offset Size Y", &obbOffset_.size.y);
  jsonManager->Register("OBB Offset Size Z", &obbOffset_.size.z);

  jsonManager->Register("OBB Offset Euler (degrees)", &obbEulerOffset_);
}

Vector3 OBBCollider::GetCenterPosition() const {
  Vector3 newPos;
  newPos.x = wt_->matWorld_.m[3][0];
  newPos.y = wt_->matWorld_.m[3][1];
  newPos.z = wt_->matWorld_.m[3][2];
  return newPos;
}

const WorldTransform &OBBCollider::GetWorldTransform() { return *wt_; }

Vector3 OBBCollider::GetEulerRotation() const {
  return wt_ ? wt_->rotate_ : Vector3{};
}

void OBBCollider::Initialize() {
  shape_ = ColliderShape::OBB;
  BaseCollider::Initialize();

  obbOffset_.center = {0.0f, 0.0f, 0.0f};
  obbOffset_.size = {1.0f, 1.0f, 1.0f};
  obbEulerOffset_ = {0.0f, 0.0f, 0.0f}; // ← 角度（度数法）
}

/// <summary>
/// クォータニオンには対応してないです
/// </summary>
void OBBCollider::Update() {
  if (!wt_) {
    return;
  }

  Matrix4x4 worldMatrix = wt_->matWorld_;

  // 位置抽出
  Vector3 worldPosition = {worldMatrix.m[3][0], worldMatrix.m[3][1],
                           worldMatrix.m[3][2]};

  // スケール抽出（各軸ベクトルの長さ）
  Vector3 worldScale = {Length(Vector3(worldMatrix.m[0][0], worldMatrix.m[0][1],
                                       worldMatrix.m[0][2])),
                        Length(Vector3(worldMatrix.m[1][0], worldMatrix.m[1][1],
                                       worldMatrix.m[1][2])),
                        Length(Vector3(worldMatrix.m[2][0], worldMatrix.m[2][1],
                                       worldMatrix.m[2][2]))};

  // スケールを除いた純粋な回転行列を直接抽出する。
  // MatrixToEuler → MakeRotateMatrixXYZ の往復を避けることで
  // ジンバルロックとスケール混入による誤計算を防ぐ。
  Matrix4x4 worldRotMatrix = {};
  worldRotMatrix.m[3][3] = 1.0f;
  if (worldScale.x > 0.0f) {
    worldRotMatrix.m[0][0] = worldMatrix.m[0][0] / worldScale.x;
    worldRotMatrix.m[0][1] = worldMatrix.m[0][1] / worldScale.x;
    worldRotMatrix.m[0][2] = worldMatrix.m[0][2] / worldScale.x;
  }
  if (worldScale.y > 0.0f) {
    worldRotMatrix.m[1][0] = worldMatrix.m[1][0] / worldScale.y;
    worldRotMatrix.m[1][1] = worldMatrix.m[1][1] / worldScale.y;
    worldRotMatrix.m[1][2] = worldMatrix.m[1][2] / worldScale.y;
  }
  if (worldScale.z > 0.0f) {
    worldRotMatrix.m[2][0] = worldMatrix.m[2][0] / worldScale.z;
    worldRotMatrix.m[2][1] = worldMatrix.m[2][1] / worldScale.z;
    worldRotMatrix.m[2][2] = worldMatrix.m[2][2] / worldScale.z;
  }

  // オフセット回転行列
  Vector3 offsetEulerRad = {DegToRad(obbEulerOffset_.x),
                            DegToRad(obbEulerOffset_.y),
                            DegToRad(obbEulerOffset_.z)};
  Matrix4x4 offsetRotMatrix = MakeRotateMatrixXYZ(offsetEulerRad);

  // 回転を合成（ワールド回転 * オフセット回転）
  Matrix4x4 combinedRotMatrix = Multiply(worldRotMatrix, offsetRotMatrix);

  // オフセット中心をワールドスケールで拡大してからワールド回転で変換する。
  // obbOffset_.center はモデルローカル空間（スケール適用前）の値なので、
  // worldScale を乗じないとスケールされたオブジェクトでセンターがズレる。
  //   誤: Transform(center, rotOnly)        ← スケール未考慮
  //   正: Transform(center * scale, rotOnly) ← スケール → 回転の順に適用
  Vector3 scaledCenter = {
      obbOffset_.center.x * worldScale.x,
      obbOffset_.center.y * worldScale.y,
      obbOffset_.center.z * worldScale.z,
  };
  Vector3 rotatedOffset = Transform(scaledCenter, worldRotMatrix);

  obb_.center = worldPosition + rotatedOffset;

  obb_.size = {obbOffset_.size.x * std::abs(worldScale.x),
               obbOffset_.size.y * std::abs(worldScale.y),
               obbOffset_.size.z * std::abs(worldScale.z)};

  // Euler変換は描画用に1回だけ（純粋な回転行列に対して行う）
  obb_.rotation = MatrixToEuler(combinedRotMatrix);

  // ローカル3軸（回転後）を明示的に設定する。
  // Ray-OBB 判定 (Intersection::IsCollision(Ray, OBB)) は obb_.rotation
  // ではなく この orientations[]
  // を読む。ここを埋めないと軸が未初期化のままになり、
  // レイが常に距離0でヒット扱い → Raycast の至近距離無視で捨てられ、
  // カメラのめり込み回避などでOBBをすり抜ける。
  // 読み手側で Normalize されるため単位ベクトルで格納する。
  obb_.orientations[0] = Transform({1.0f, 0.0f, 0.0f}, combinedRotMatrix);
  obb_.orientations[1] = Transform({0.0f, 1.0f, 0.0f}, combinedRotMatrix);
  obb_.orientations[2] = Transform({0.0f, 0.0f, 1.0f}, combinedRotMatrix);
}

void OBBCollider::Draw() {
  line_->Reset();
  line_->DrawOBB(obb_.center, obb_.rotation, obb_.size);
  line_->DrawLine();
}
