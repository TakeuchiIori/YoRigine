#include "BaseCollider.h"
#include "CollisionManager.h"
// Engine

void BaseCollider::Initialize()
{
	if (line_) {
		delete line_;
		line_ = nullptr;
	}

	line_ = new Line();
	line_->Initialize();
	line_->SetCamera(camera_);
	CollisionManager::GetInstance()->AddCollider(this);
}

BaseCollider::~BaseCollider()
{
	CollisionManager::GetInstance()->RemoveCollider(this);
	delete line_;
	line_ = nullptr;
}








