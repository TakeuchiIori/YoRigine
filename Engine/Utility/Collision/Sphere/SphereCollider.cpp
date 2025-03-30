#include "SphereCollider.h"

SphereCollider::~SphereCollider()
{
	BaseCollider::~BaseCollider();
}

void SphereCollider::InitJson(JsonManager* jsonManager)
{
	// 衝突球のオフセットや半径を JSON に登録
	jsonManager->SetCategory("Colliders");
	jsonManager->Register("Sphere Offset X", &sphereOffset_.center.x);
	jsonManager->Register("Sphere Offset Y", &sphereOffset_.center.y);
	jsonManager->Register("Sphere Offset Z", &sphereOffset_.center.z);
	jsonManager->Register("Sphere Radius", &radius_);
}

void SphereCollider::Initialize()
{
	BaseCollider::Initialize();

	sphere_.center = { 0.0f,0.0f,0.0f };
	sphereOffset_.center = { 0.0f,0.0f,0.0f };
	sphere_.radius = 0.0f;
	sphereOffset_.radius = 0.0f;
	// 衝突半径の設定
	radius_ = 1.0f;
}

void SphereCollider::Update()
{
	sphere_.center = GetCenterPosition() + sphereOffset_.center;
	sphere_.radius = radius_ + sphereOffset_.radius;
}

void SphereCollider::Draw()
{
	line_->DrawSphere(sphere_.center, sphere_.radius, 32);
	line_->DrawLine();
}

Vector3 SphereCollider::GetCenterPosition() const {

	Vector3 newPos;
	newPos.x = worldTransform_->matWorld_.m[3][0];
	newPos.y = worldTransform_->matWorld_.m[3][1];
	newPos.z = worldTransform_->matWorld_.m[3][2];
	return newPos;
}

Matrix4x4 SphereCollider::GetWorldMatrix() const {
	return worldTransform_ ? worldTransform_->matWorld_ : MakeIdentity4x4();
}

Vector3 SphereCollider::GetEulerRotation() const {
	return worldTransform_ ? worldTransform_->rotation_ : Vector3{};
}
