#include "Collider.h"
#include "CollisionManager.h"
// Engine


void Collider::Initialize()
{
	line_ = new Line();
	line_->Initialize();
	line_->SetCamera(camera_);
	CollisionManager::GetInstance()->AddCollider(this);
}

Collider::~Collider()
{
	CollisionManager::GetInstance()->RemoveCollider(this);
}








