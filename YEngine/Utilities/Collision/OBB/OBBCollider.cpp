#include "OBBCollider.h"
#include "Matrix4x4.h"

void OBBCollider::InitJson(YoRigine::JsonManager* jsonManager)
{
	jsonManager->SetCategory("Colliders");

	jsonManager->Register("OBB Offset Center X", &obbOffset_.center.x);
	jsonManager->Register("OBB Offset Center Y", &obbOffset_.center.y);
	jsonManager->Register("OBB Offset Center Z", &obbOffset_.center.z);

	jsonManager->Register("OBB Offset Size X", &obbOffset_.size.x);
	jsonManager->Register("OBB Offset Size Y", &obbOffset_.size.y);
	jsonManager->Register("OBB Offset Size Z", &obbOffset_.size.z);

	jsonManager->Register("OBB Offset Euler (degrees)", &obbEulerOffset_);
}

Vector3 OBBCollider::GetCenterPosition() const
{
	Vector3 newPos;
	newPos.x = wt_->matWorld_.m[3][0];
	newPos.y = wt_->matWorld_.m[3][1];
	newPos.z = wt_->matWorld_.m[3][2];
	return newPos;
}

const WorldTransform& OBBCollider::GetWorldTransform()
{
	return *wt_;
}

Vector3 OBBCollider::GetEulerRotation() const
{
	return wt_ ? wt_->rotate_ : Vector3{};
}

void OBBCollider::Initialize()
{
	BaseCollider::Initialize();

	obbOffset_.center = { 0.0f, 0.0f, 0.0f };
	obbOffset_.size = { 1.0f, 1.0f, 1.0f };
	obbEulerOffset_ = { 0.0f, 0.0f, 0.0f }; // ← 角度（度数法）
}

/// <summary>
/// クォータニオンには対応してないです
/// </summary>
void OBBCollider::Update()
{
	if (!wt_) {
		return;
	}

	// 親子関係を考慮したワールド行列から値を取得
	Matrix4x4 worldMatrix = wt_->matWorld_;

	// ワールド行列から位置、回転、スケールを抽出
	Vector3 worldPosition = {
		worldMatrix.m[3][0],
		worldMatrix.m[3][1],
		worldMatrix.m[3][2]
	};

	Vector3 worldRotation = MatrixToEuler(worldMatrix);

	// スケールを抽出（各軸のベクトルの長さ）
	Vector3 worldScale = {
		Length(Vector3(worldMatrix.m[0][0], worldMatrix.m[0][1], worldMatrix.m[0][2])),
		Length(Vector3(worldMatrix.m[1][0], worldMatrix.m[1][1], worldMatrix.m[1][2])),
		Length(Vector3(worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2]))
	};

	// オフセットを適用したOBBの中心位置を計算
	Vector3 offsetEulerRad = {
		DegToRad(obbEulerOffset_.x),
		DegToRad(obbEulerOffset_.y),
		DegToRad(obbEulerOffset_.z)
	};

	// オフセット回転行列を作成
	Matrix4x4 offsetRotMatrix = MakeRotateMatrixXYZ(offsetEulerRad);

	// ワールド回転行列を作成
	Matrix4x4 worldRotMatrix = MakeRotateMatrixXYZ(worldRotation);

	// 回転を合成（ワールド回転 * オフセット回転）
	Matrix4x4 combinedRotMatrix = Multiply(worldRotMatrix, offsetRotMatrix);

	// オフセット位置をワールド回転で変換
	Vector3 rotatedOffset = Transform(obbOffset_.center, worldRotMatrix);

	// 最終的なOBBの中心位置
	obb_.center = worldPosition + rotatedOffset;

	// サイズをスケールに応じて調整
	obb_.size = {
		obbOffset_.size.x * std::abs(worldScale.x),
		obbOffset_.size.y * std::abs(worldScale.y),
		obbOffset_.size.z * std::abs(worldScale.z)
	};

	// 最終的な回転
	obb_.rotation = MatrixToEuler(combinedRotMatrix);

}

void OBBCollider::Draw()
{
	line_->DrawOBB(obb_.center, obb_.rotation, obb_.size);
	line_->DrawLine();
}
