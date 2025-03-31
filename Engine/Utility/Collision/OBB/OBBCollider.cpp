#include "OBBCollider.h"
#include "Matrix4x4.h"


OBBCollider::~OBBCollider()
{
	BaseCollider::~BaseCollider();
}
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
	// スケールとアンカーポイントを取得
	Vector3 scale = GetScale();
	Vector3 anchor = GetAnchorPoint();
	Vector3 center = GetCenterPosition();

	// サイズをスケールに応じて拡大
	Vector3 scaledSize = {
		obbOffset_.size.x * std::abs(scale.x),
		obbOffset_.size.y * std::abs(scale.y),
		obbOffset_.size.z * std::abs(scale.z),
	};

	// アンカーポイント補正（AABBと同様）
	Vector3 anchorOffset = {
		scaledSize.x * (anchor.x - 0.5f),
		scaledSize.y * (anchor.y - 0.5f),
		scaledSize.z * (anchor.z - 0.5f)
	};

	// OBBの中心はアンカーポイントを考慮した位置
	obb_.center = /*center*/ /*- anchorOffset + */Transform(obbOffset_.center, GetWorldMatrix());

	// サイズ設定（すでにスケーリングされてる）
	obb_.size = scaledSize;

	// 回転行列の合成（ローカル→ワールド）
	Vector3 offsetEulerRad = {
		DegToRad(obbEulerOffset_.x),
		DegToRad(obbEulerOffset_.y),
		DegToRad(obbEulerOffset_.z)
	};
	Vector3 worldEulerRad = GetEulerRotation();

	Matrix4x4 rotOffset = MakeRotateMatrixXYZ(offsetEulerRad);
	Matrix4x4 rotWorld = MakeRotateMatrixXYZ(worldEulerRad);

	Matrix4x4 combinedRot = Multiply(rotWorld, rotOffset);

	obb_.rotation = MatrixToEuler(combinedRot);
}


void OBBCollider::Draw()
{
	line_->DrawOBB(obb_.center, obb_.rotation, obb_.size);
	line_->DrawLine();
}

Vector3 OBBCollider::GetScale() const
{
	return worldTransform_->scale_;
}

Vector3 OBBCollider::GetAnchorPoint() const
{
	return worldTransform_->anchorPoint_;
}

Vector3 OBBCollider::GetCenterPosition() const {

	Vector3 newPos;
	newPos.x = worldTransform_->matWorld_.m[3][0];
	newPos.y = worldTransform_->matWorld_.m[3][1];
	newPos.z = worldTransform_->matWorld_.m[3][2];
	return newPos;
}

Matrix4x4 OBBCollider::GetWorldMatrix() const {
	return worldTransform_ ? worldTransform_->matWorld_ : MakeIdentity4x4();
}

Vector3 OBBCollider::GetEulerRotation() const {
	return worldTransform_ ? worldTransform_->rotation_ : Vector3{};
}
