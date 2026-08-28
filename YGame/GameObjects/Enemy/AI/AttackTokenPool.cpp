#include "AttackTokenPool.h"

#include <algorithm>

AttackTokenPool *AttackTokenPool::current_ = nullptr;

void AttackTokenPool::Update(float dt) {
  for (auto it = cooldowns_.begin(); it != cooldowns_.end();) {
    it->second -= dt;
    if (it->second <= 0.0f) {
      it = cooldowns_.erase(it);
    } else {
      ++it;
    }
  }
}

bool AttackTokenPool::TryAcquire(const BaseEnemy *enemy) {
  if (!enemy)
    return false;

  // 無効化されている間は誰でも攻撃できる
  if (!enabled_)
    return true;

  // 既に持っているならそのまま
  if (Holds(enemy))
    return true;

  // 直前まで攻撃していた個体は少し待たせて、他の敵に順番を回す
  if (cooldowns_.find(enemy) != cooldowns_.end())
    return false;

  if (static_cast<int>(holders_.size()) >= maxTokens_)
    return false;

  holders_.push_back(enemy);
  return true;
}

void AttackTokenPool::Release(const BaseEnemy *enemy) {
  if (!enemy)
    return;

  auto it = std::find(holders_.begin(), holders_.end(), enemy);
  if (it == holders_.end())
    return;

  holders_.erase(it);
  if (reacquireCooldown_ > 0.0f) {
    cooldowns_[enemy] = reacquireCooldown_;
  }
}

void AttackTokenPool::Forget(const BaseEnemy *enemy) {
  if (!enemy)
    return;

  auto it = std::find(holders_.begin(), holders_.end(), enemy);
  if (it != holders_.end()) {
    holders_.erase(it);
  }
  cooldowns_.erase(enemy);
}

void AttackTokenPool::Clear() {
  holders_.clear();
  cooldowns_.clear();
}

bool AttackTokenPool::Holds(const BaseEnemy *enemy) const {
  return std::find(holders_.begin(), holders_.end(), enemy) != holders_.end();
}

float AttackTokenPool::GetCooldownRemaining(const BaseEnemy *enemy) const {
  auto it = cooldowns_.find(enemy);
  return (it != cooldowns_.end()) ? it->second : 0.0f;
}
