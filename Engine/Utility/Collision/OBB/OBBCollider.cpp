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

	// ワールド行列は位置と中心の変換だけに使う（回転は使わない！）
	Matrix4x4 worldMatrix = GetWorldMatrix();

	// OBB中心の計算（親オブジェクトの座標系に対して）
	obb_.center = Transform(obbOffset_.center, worldMatrix);
	obb_.size = obbOffset_.size;

	// ① オフセット回転をQuaternionに変換
	Quaternion offsetQ = EulerToQuaternion({
		DegToRad(obbEulerOffset_.x),
		DegToRad(obbEulerOffset_.y),
		DegToRad(obbEulerOffset_.z)
		});

	// ② 親の回転（行列からではなく、オイラー角から直接作る！）
	Vector3 worldEulerDeg = GetEulerRotation(); // ← worldTransform_.rotation_
	Quaternion worldQ = EulerToQuaternion({
		DegToRad(worldEulerDeg.x),
		DegToRad(worldEulerDeg.y),
		DegToRad(worldEulerDeg.z)
		});

	// ③ 回転合成（親 → オフセット）
	obb_.rotation = worldQ * offsetQ;


}

void OBBCollider::Draw()
{
	line_->DrawOBB(obb_.center, obb_.rotation, obb_.size);
	line_->DrawLine();
}
