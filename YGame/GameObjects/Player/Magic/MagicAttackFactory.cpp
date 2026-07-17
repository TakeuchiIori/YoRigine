#include "MagicAttackFactory.h"

std::unique_ptr<MagicAttackInstance> MagicAttackFactory::Create(const MagicActionData& action, Player* owner)
{
	// 生成差分はFactoryに集約する。現時点では全タイプをSphere判定の最小実体へ畳むが、
	// Projectile / Beam / Area が専用挙動を持った時も呼び出し側のControllerは変えない。
	auto instance = std::make_unique<MagicAttackInstance>();
	instance->Initialize(action, owner);
	return instance;
}
