#include "SphereCollider.h"
#include "MathFunc.h"
#include "Vector3.h"

void SphereCollider::InitJson(JsonManager* jsonManager)
{
	// 衝突球のオフセットや半径を JSON に登録
	jsonManager->SetCategory("Colliders");
	jsonManager->Register("Collider Offset X", &sphereOffset_.center.x);
	jsonManager->Register("Collider Offset Y", &sphereOffset_.center.y);
	jsonManager->Register("Collider Offset Z", &sphereOffset_.center.z);
	jsonManager->Register("Collider Radius", &radius_);
}

Vector3 SphereCollider::GetCenterPosition() const
{
	Vector3 newPos;
	newPos.x = wt_->matWorld_.m[3][0];
	newPos.y = wt_->matWorld_.m[3][1];
	newPos.z = wt_->matWorld_.m[3][2];
	return newPos;
}

const WorldTransform& SphereCollider::GetWorldTransform()
{
	return *wt_;
}

Vector3 SphereCollider::GetEulerRotation() const
{
	return wt_ ? wt_->rotation_ : Vector3{};
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
	//radius_ = GetWorldTransform().scale_.x;
	sphere_.center = GetCenterPosition() + sphereOffset_.center;
	sphere_.radius = radius_ + sphereOffset_.radius;
}

void SphereCollider::Draw()
{
	line_->DrawSphere(sphere_.center, sphere_.radius, 32);
	line_->DrawLine();
}
