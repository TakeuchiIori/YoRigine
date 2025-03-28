#include "SphereCollider.h"

void SphereCollider::Initialize()
{
	line_ = new Line();
	line_->Initialize();
	Collider::AddCollider();
}

void SphereCollider::Draw(Camera* camera)
{
	line_->DrawSphere(sphere_.center, sphere_.radius, 32);
	line_->DrawLine();
}
