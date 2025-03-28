#include "SphereCollider.h"

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
