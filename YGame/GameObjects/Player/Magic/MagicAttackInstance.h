#pragma once

#include "MagicActionData.h"
#include "WorldTransform/WorldTransform.h"
#include "Collision/Sphere/SphereCollider.h"

#include <memory>
#include <unordered_set>

class BaseCollider;
class Player;

// ============================================================
// 実行中の魔法攻撃
// MagicActionData を 1 回再生する単位として、移動・スケール・判定寿命を持つ。
// PlayerMagicController に判定処理を置くと攻撃種別ごとの更新分岐で膨らむため、
// 「発生した攻撃自身」が当たり方を管理する。
// ============================================================
class MagicAttackInstance {
public:
	void Initialize(const MagicActionData& action, Player* owner);
	void Update(float deltaTime);
	void DrawCollision();

	bool IsAlive() const { return alive_; }

	void OnEnterCollision(BaseCollider* self, BaseCollider* other);
	void OnCollision(BaseCollider* self, BaseCollider* other);
	void OnExitCollision(BaseCollider* self, BaseCollider* other);
	void OnDirectionCollision(BaseCollider* self, BaseCollider* other, HitDirection dir);
	void OnEnterDirectionCollision(BaseCollider* self, BaseCollider* other, HitDirection dir);

private:
	Vector3 ResolveOrigin(Player& owner) const;
	Vector3 ResolveForward(Player& owner) const;
	Vector3 ResolveTarget(Player& owner, const Vector3& origin) const;
	void ApplyHit(BaseCollider* other);

private:
	MagicActionData action_;
	Player* owner_ = nullptr;
	WorldTransform wt_;
	std::shared_ptr<SphereCollider> collider_;
	Vector3 origin_{};
	Vector3 target_{};
	float elapsedTime_ = 0.0f;
	bool alive_ = false;
	std::unordered_set<BaseCollider*> hitColliders_;
};
