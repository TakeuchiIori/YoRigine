#include "OBBCollider.h"

void OBBCollider::InitJson(JsonManager* jsonManager)
{
}

void OBBCollider::Initialize()
{
	Collider::Initialize();
}

void OBBCollider::Draw()
{
	line_->DrawOBB(obb_.center, obb_.rotation, obb_.size);
	line_->DrawLine();
}
