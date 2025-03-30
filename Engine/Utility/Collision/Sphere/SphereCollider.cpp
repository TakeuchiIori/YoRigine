#include "SphereCollider.h"

void SphereCollider::InitJson(JsonManager* jsonManager)
{
	// 衝突球のオフセットや半径を JSON に登録
	jsonManager->SetCategory("Colliders");
	jsonManager->Register("Collider Offset X", &sphereOffset_.center.x);
	jsonManager->Register("Collider Offset Y", &sphereOffset_.center.y);
	jsonManager->Register("Collider Offset Z", &sphereOffset_.center.z);
	jsonManager->Register("Collider Radius", &radius_);
}

void SphereCollider::Initialize()
{
	Collider::Initialize();

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

void SphereCollider::InitJson()
{

}
