#include "OBBCollider.h"
#include "Matrix4x4.h"

void OBBCollider::InitJson(JsonManager* jsonManager)
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

void OBBCollider::Initialize()
{
	Collider::Initialize();

	obbOffset_.center = { 0.0f, 0.0f, 0.0f };
	obbOffset_.size = { 1.0f, 1.0f, 1.0f };
	obbEulerOffset_ = { 0.0f, 0.0f, 0.0f }; // ← 角度（度数法）
}

void OBBCollider::Update()
{

	// ワールド行列は位置と中心の変換だけに使う（回転は使わない）
	Matrix4x4 worldMatrix = GetWorldMatrix();

	// OBB中心とサイズの更新
	obb_.center = Transform(obbOffset_.center, worldMatrix);
	obb_.size = obbOffset_.size;

	// ① オフセット回転をラジアンに変換
	Vector3 offsetEulerRad = {
		DegToRad(obbEulerOffset_.x),
		DegToRad(obbEulerOffset_.y),
		DegToRad(obbEulerOffset_.z)
	};

	// ② ワールド回転（オブジェクトの回転）をラジアンに変換
	Vector3 worldEulerDeg = GetEulerRotation(); // ← worldTransform_.rotation_
	Vector3 worldEulerRad = {
		DegToRad(worldEulerDeg.x),
		DegToRad(worldEulerDeg.y),
		DegToRad(worldEulerDeg.z)
	};

	// ③ 回転を加算（合成）して保存
	obb_.rotation = worldEulerRad + offsetEulerRad;

}

void OBBCollider::Draw()
{
	line_->DrawOBB(obb_.center, obb_.rotation, obb_.size);
	line_->DrawLine();
}
