#include "OBBCollider.h"

void OBBCollider::Draw(Camera* camera)
{
	line_->DrawOBB(obb_.center, obb_.rotation, obb_.size);
}
