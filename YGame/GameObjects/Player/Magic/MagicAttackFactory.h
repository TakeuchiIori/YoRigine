#pragma once

#include "MagicActionData.h"
#include "MagicAttackInstance.h"

#include <memory>

class Player;

// ============================================================
// 魔法攻撃生成の入口
// Controller は「いつ撃つか」だけを持ち、Beam / Projectile / Area などの
// 生成差分はここへ寄せる。入力管理に魔法種別の分岐を溜めないための境界。
// ============================================================
class MagicAttackFactory {
public:
	static std::unique_ptr<MagicAttackInstance> Create(const MagicActionData& action, Player* owner);
};
