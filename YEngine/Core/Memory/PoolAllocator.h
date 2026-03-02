#pragma once
#include <cstdint>
#include <cstddef>  // offsetof 用
#include <cassert>

template<class T, size_t MAXSIZE>
/// <summary>
/// メモリをプールして効率的に確保・解放するアロケータ
/// </summary>
class PoolAllocator
{
	// ── ノード定義 ──
	union Node {
		alignas(T) char storage[sizeof(T)];
		Node* next;   // in-class 初期化子は外す
	};

public:
	///************************* 基本的な関数 *************************///

	/// <summary>
	/// コンストラクタ
	/// </summary>
	PoolAllocator() noexcept
		: freeList_(nullptr)
		, nodes_{}     // 配列全体をゼロ初期化
	{
		// freeList を構築
		for (size_t i = 0; i < MAXSIZE - 1; ++i) {
			nodes_[i].next = &nodes_[i + 1];
		}
		nodes_[MAXSIZE - 1].next = nullptr;
		freeList_ = &nodes_[0];
	}

	/// <summary>
	/// デストラクタ
	/// </summary>
	~PoolAllocator() { Clear(); }

	/// <summary>
	/// メモリを借りる
	/// </summary>
	/// <returns></returns>
	T* Alloc()
	{
		if (!freeList_) return nullptr;
		Node* node = freeList_;
		freeList_ = node->next;
		return new (&node->storage) T();
	}

	/// <summary>
	/// メモリを返却
	/// </summary>
	/// <param name="ptr"></param>
	void Free(T* ptr)
	{
		if (!ptr) return;
		// storage 先頭アドレスから Node* を逆算
		Node* node = reinterpret_cast<Node*>(
			reinterpret_cast<char*>(ptr) - offsetof(Node, storage));
		assert(node >= &nodes_[0] && node < &nodes_[MAXSIZE]);
		// デストラクタ呼び出し
		ptr->~T();
		// リストに戻す
		node->next = freeList_;
		freeList_ = node;
	}

	/// <summary>
	/// 全オブジェクトを破棄してプールをリセット
	/// </summary>
	void Clear()
	{
		// アクティブなオブジェクトをすべてデストラクト
		for (size_t i = 0; i < MAXSIZE; ++i) {
			Node* node = &nodes_[i];
			if (!IsInFreeList(node)) {
				T* obj = reinterpret_cast<T*>(&node->storage);
				obj->~T();
			}
		}
		// freeList を再構築
		for (size_t i = 0; i < MAXSIZE - 1; ++i) {
			nodes_[i].next = &nodes_[i + 1];
		}
		nodes_[MAXSIZE - 1].next = nullptr;
		freeList_ = &nodes_[0];
	}

private:
	///************************* メンバ変数 *************************///

	/// <summary>
	/// ノードがフリーリスト内にあるか調べる
	/// </summary>
	/// <param name="target"></param>
	/// <returns></returns>
	bool IsInFreeList(Node* target) const
	{
		for (Node* node = freeList_; node; node = node->next) {
			if (node == target) return true;
		}
		return false;
	}

private:
	Node* freeList_;
	Node  nodes_[MAXSIZE];
};
