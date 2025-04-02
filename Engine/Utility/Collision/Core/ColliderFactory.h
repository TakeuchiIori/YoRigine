#pragma once
#include "BaseCollider.h"
#include "ColliderPool.h"
#include "WorldTransform/WorldTransform.h"
#include "Systems/Camera/Camera.h"

class ColliderFactory
{
public:

	template <typename T,typename TObject>
	static std::shared_ptr<T> Create(
		TObject* owner,
		const WorldTransform* worldTransform,
		Camera* camera,
		uint32_t typeID) {
		static_assert(std::is_base_of<BaseCollider, T>::value, "T must be derived from BaseCollider");

		auto collider = ColliderPool::GetInstance()->GetCollider<T>();

		// 必要なものを初期化
		collider->SetTransform(worldTransform);
		collider->SetCamera(camera);
		collider->Initialize();
		collider->SetTypeID(typeID);

		// コールバック関数の登録
		collider->SetOnEnterCollision([owner](BaseCollider* self, BaseCollider* other) {
			if (owner) {
				owner->OnEnterCollision(self, other);
			}
			});
		collider->SetOnCollision([owner](BaseCollider* self, BaseCollider* other) {
			if (owner) {
				owner->OnCollision(self, other);
			}
			});
		collider->SetOnExitCollision([owner](BaseCollider* self, BaseCollider* other) {
			owner->OnExitCollision(self, other);
			});

		return collider;
	}
};

