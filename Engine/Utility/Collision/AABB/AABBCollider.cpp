#include "AABBCollider.h"

void AABBCollider::Initialzie()
{

	Collider::Initialize();
}

void AABBCollider::Draw()
{
	line_->DrawAABB(aabb_.min, aabb_.max);
	line_->DrawLine();
}
