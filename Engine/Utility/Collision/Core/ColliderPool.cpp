#include "ColliderPool.h"

ColliderPool* ColliderPool::GetInstance()
{
	static ColliderPool instance;
	return &instance;
}

void ColliderPool::Clear()
{
	for (auto& [_, it] : pool_) {
		for (auto& collider : it) {
			collider->SetActive(false);
		}
	}
}
