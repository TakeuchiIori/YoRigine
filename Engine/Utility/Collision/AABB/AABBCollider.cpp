#include "AABBCollider.h"

void AABBCollider::InitJson(JsonManager* jsonManager)
{
}

void AABBCollider::Initialzie()
{

	Collider::Initialize();
}

void AABBCollider::Update()
{
}

void AABBCollider::Draw()
{
	line_->DrawAABB(aabb_.min, aabb_.max);
	line_->DrawLine();
}
