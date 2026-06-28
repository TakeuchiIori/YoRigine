#include "BaseCollider.h"
#include "CollisionManager.h"
// Engine

void BaseCollider::Initialize()
{
	line_ = std::make_unique<Line>();
	line_->Initialize();
	line_->SetCamera(camera_);
	YoRigine::CollisionManager::GetInstance()->AddCollider(this);
}

BaseCollider::~BaseCollider()
{
	YoRigine::CollisionManager::GetInstance()->RemoveCollider(this);
	line_ = nullptr;
}








