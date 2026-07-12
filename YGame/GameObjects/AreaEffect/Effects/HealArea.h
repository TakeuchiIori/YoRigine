#pragma once

#include "../AreaEffectBase.h"

// ============================================================
// 回復エリア：円内にいる対象を tick 間隔ごとに回復する。
//   使い方: Setup(center, radius, 0.5f); SetHealPerTick(3.0f);
// ============================================================
class HealArea : public AreaEffectBase {
public:
	void  SetHealPerTick(float h) { healPerTick_ = h; }
	float GetHealPerTick() const  { return healPerTick_; }

protected:
	void OnStay(IAreaEffectTarget* target) override {
		if (target && target->IsEffectTargetAlive()) {
			target->ApplyAreaHeal(healPerTick_);
		}
	}

private:
	float healPerTick_ = 3.0f;   // 1 tick あたりの回復量
};
