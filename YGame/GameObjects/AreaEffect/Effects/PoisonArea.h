#pragma once

#include "../AreaEffectBase.h"

// ============================================================
// 毒沼エリア：円内にいる対象へ tick 間隔ごとにダメージを与える。
//   使い方: Setup(center, radius, 0.5f); SetDamagePerTick(5.0f);
// ============================================================
class PoisonArea : public AreaEffectBase {
public:
	void  SetDamagePerTick(float d) { damagePerTick_ = d; }
	float GetDamagePerTick() const  { return damagePerTick_; }

protected:
	void OnStay(IAreaEffectTarget* target) override {
		if (target && target->IsEffectTargetAlive()) {
			target->ApplyAreaDamage(damagePerTick_);
		}
	}

private:
	float damagePerTick_ = 5.0f;   // 1 tick あたりのダメージ
};
