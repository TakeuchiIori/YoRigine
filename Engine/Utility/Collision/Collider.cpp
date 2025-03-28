#include "Collider.h"
#include "CollisionManager.h"
// Engine


void Collider::AddCollider() {
	CollisionManager::GetInstance()->AddCollider(this);
}





