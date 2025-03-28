#include "OBBCollider.h"

void OBBCollider::Initialize()
{
	line_ = new Line();
	line_->Initialize();
	Collider::AddCollider();
}

void OBBCollider::Draw(Camera* camera)
{
	line_->DrawOBB(obb_.center, obb_.rotation, obb_.size);
}
