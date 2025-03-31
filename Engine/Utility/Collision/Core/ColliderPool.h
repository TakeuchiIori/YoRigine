#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <type_traits>

#include "BaseCollider.h"

class ColliderPool {
public:

	/// <summary>
	/// シングルトン
	/// </summary>
	/// <returns></returns>
	static ColliderPool* GetInstance();


	template <typename T>
	std::shared_ptr<T> GetCollider();


	void Clear();


private:
	~ColliderPool() = default;

	std::unordered_map<std::type_index, std::vector<std::shared_ptr<BaseCollider>>> pool_;

};

template<typename T>
inline std::shared_ptr<T> ColliderPool::GetCollider()
{
	static_assert(std::is_base_of<BaseCollider, T>::value, "T must be derived from BaseCollider");

	auto& it = pool_[typeid(T)];
	for (auto& collider : it) {
		if (!collider->IsActive()) {
			collider->SetActive(true);
			return std::static_pointer_cast<T>(collider);
		}
	}

	// なければ新規作成
	auto newCollider = std::make_shared<T>();
	newCollider->SetActive(true);
	it.push_back(newCollider);
	return newCollider;
}
