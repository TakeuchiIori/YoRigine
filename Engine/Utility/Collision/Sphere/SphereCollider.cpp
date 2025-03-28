#include "SphereCollider.h"

void SphereCollider::Draw(Camera* camera)
{
	line_->DrawSphere(sphere_.center, sphere_.radius, 32);
}
