#include "AABBCollider.h"

void AABBCollider::Initialzie()
{
	line_ = new Line();
	line_->Initialize();

	Collider::AddCollider();
}

void AABBCollider::Draw(Camera* camera)
{
	line_->DrawAABB(aabb_.min, aabb_.max);
}
