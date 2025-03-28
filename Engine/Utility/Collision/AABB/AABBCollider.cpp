#include "AABBCollider.h"

void AABBCollider::Draw(Camera* camera)
{
	line_->DrawAABB(aabb_.min, aabb_.max);
}
