#pragma once

// C++
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <type_traits>

// Engine
#include "BaseCollider.h"
#include "Memory/PoolAllocator.h"

// コライダー型ごとのプールサイズ定義
constexpr size_t DEFAULT_COLLIDER_POOL_SIZE = 1024;

// コライダーをプール管理するクラス
// PoolAllocatorを使用して再利用可能なコライダーを保持し、生成コストを削減する
// shared_ptrのカスタムデリータにより自動でプールに返却される
class ColliderPool
{
public:
	///************************* シングルトン *************************///

	// インスタンス取得
	static ColliderPool* GetInstance();

public:
	///************************* コライダー取得 *************************///

	// コライダーを取得または再利用
	// shared_ptrで返すため、スコープを抜けると自動的にプールに返却される
	template <typename T>
	std::shared_ptr<T> GetCollider();

	// コライダーをプールに返却（内部用・通常は自動で呼ばれる）
	template <typename T>
	void ReturnCollider(T* collider);

public:
	///************************* プール管理 *************************///

	// 全コライダーのクリア
	void Clear();

private:
	///************************* 内部管理 *************************///

	ColliderPool() = default;
	~ColliderPool() = default;

	// コピー・ムーブ禁止
	ColliderPool(const ColliderPool&) = delete;
	ColliderPool& operator=(const ColliderPool&) = delete;

	// 型ごとのアロケータを保持する基底クラス
	struct IAllocatorHolder {
		virtual ~IAllocatorHolder() = default;
	};

	// 実際のアロケータを保持するテンプレートクラス
	template<typename T>
	struct AllocatorHolder : IAllocatorHolder {
		PoolAllocator<T, DEFAULT_COLLIDER_POOL_SIZE> allocator;
	};

	// 型ごとのアロケータホルダーを管理
	std::unordered_map<std::type_index, IAllocatorHolder*> allocators_;

	// 指定型のアロケータを取得（存在しなければ作成）
	template<typename T>
	AllocatorHolder<T>* GetOrCreateAllocator();
};

// コライダー取得処理
template<typename T>
inline std::shared_ptr<T> ColliderPool::GetCollider()
{
	static_assert(std::is_base_of<BaseCollider, T>::value, "T must be derived from BaseCollider");

	// アロケータを取得
	auto* holder = GetOrCreateAllocator<T>();
	if (!holder) return nullptr;

	// メモリを取得
	T* collider = holder->allocator.Alloc();
	if (!collider) return nullptr;

	collider->SetActive(true);

	// カスタムデリータを使ってshared_ptrを作成
	// shared_ptrが破棄される時に自動的にプールに返却される
	return std::shared_ptr<T>(collider, [](T* ptr) {
		if (ptr) {
			ColliderPool::GetInstance()->ReturnCollider<T>(ptr);
		}
		});
}

// コライダー返却処理
template<typename T>
inline void ColliderPool::ReturnCollider(T* collider)
{
	static_assert(std::is_base_of<BaseCollider, T>::value, "T must be derived from BaseCollider");

	if (!collider) return;

	// 非アクティブ化
	collider->SetActive(false);

	// アロケータを取得
	auto* holder = GetOrCreateAllocator<T>();
	if (!holder) return;

	// メモリを返却
	holder->allocator.Free(collider);
}

// アロケータ取得または作成
template<typename T>
inline ColliderPool::AllocatorHolder<T>* ColliderPool::GetOrCreateAllocator()
{
	std::type_index typeIdx(typeid(T));

	// 既存のアロケータがあれば返す
	auto it = allocators_.find(typeIdx);
	if (it != allocators_.end()) {
		return static_cast<AllocatorHolder<T>*>(it->second);
	}

	// 新規作成
	auto* holder = new AllocatorHolder<T>();
	allocators_[typeIdx] = holder;
	return holder;
}