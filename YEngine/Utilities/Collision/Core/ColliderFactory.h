#pragma once

// Engine
#include "BaseCollider.h"
#include "ColliderPool.h"
#include "WorldTransform/WorldTransform.h"
#include "Systems/Camera/Camera.h"

// C++
#include <memory>

// コライダーを生成し初期化するクラス
// プールからコライダーを取得し、所有オブジェクトのコールバックを自動登録する
// shared_ptrで返すため、スコープを抜けると自動的にプールに返却される
class ColliderFactory
{
public:
	///************************* コライダー生成 *************************///

	// コライダーを生成して初期化
	// T は BaseCollider を継承している必要がある
	// TObject は OnEnterCollision などのコールバック関数を持つクラス
	// 戻り値の shared_ptr が破棄されると、自動的にプールに返却される
	template <typename T, typename TObject>
	static std::shared_ptr<T> Create(
		TObject* owner,
		 WorldTransform* worldTransform,
		Camera* camera,
		uint32_t typeID)
	{
		static_assert(std::is_base_of<BaseCollider, T>::value, "T must be derived from BaseCollider");

		// コライダープールから取得（カスタムデリータ付きshared_ptrで返される）
		auto collider = ColliderPool::GetInstance()->GetCollider<T>();
		if (!collider) {
			return nullptr; // プールが枯渇している場合
		}

		// 各種初期設定
		collider->SetWT(worldTransform);
		collider->SetCamera(camera);
		collider->Initialize();
		collider->SetTypeID(typeID);

		// コールバック登録
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
			if (owner) {
				owner->OnExitCollision(self, other);
			}
			});

		collider->SetOnDirectionCollision([owner](BaseCollider* self, BaseCollider* other, HitDirection dir) {
			if (owner) {
				owner->OnDirectionCollision(self, other, dir);
			}
			});

		collider->SetOnEnterDirectionCollision([owner](BaseCollider* self, BaseCollider* other, HitDirection dir) {
			if (owner) {
				owner->OnEnterDirectionCollision(self, other, dir);
			}
			});

		return collider;
	}
};