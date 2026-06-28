#include "ColliderPool.h"

ColliderPool* ColliderPool::GetInstance()
{
	static ColliderPool instance;
	return &instance;
}

void ColliderPool::Clear()
{
	// すべてのアロケータをクリア
	for (auto& [typeIdx, holder] : allocators_) {
		if (holder) {
			delete holder;
		}
	}
	allocators_.clear();
}