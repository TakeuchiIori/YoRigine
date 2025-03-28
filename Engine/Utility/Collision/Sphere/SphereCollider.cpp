#include "SphereCollider.h"

void SphereCollider::Initialize()
{
	Collider::Initialize();

	sphere_.center = GetCenterPosition();
	sphere_.radius = 1.0f;
}

void SphereCollider::Update()
{
}

void SphereCollider::Draw()
{
	line_->DrawSphere(sphere_.center, sphere_.radius, 32);
	line_->DrawLine();
}
