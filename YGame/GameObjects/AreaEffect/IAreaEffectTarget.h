#pragma once

// Math
#include "Vector3.h"

// C++
#include <string>

// ============================================================
// エリアエフェクト（毒・回復・バフ等）の効果対象インターフェース。
//   Player / FieldEnemy / BattleEnemy などが継承し、
//   AreaEffect 側は本インターフェース越しに統一的に扱う。
//   効果フックは必要なものだけ override すればよい（既定は空実装）。
// ============================================================
class IAreaEffectTarget {
public:
	virtual ~IAreaEffectTarget() = default;

	///--- 必須 ---///

	// 円内判定に使うワールド座標
	virtual Vector3 GetEffectPosition() const = 0;

	// 生存しているか（false の対象には効果を適用しない）
	virtual bool IsEffectTargetAlive() const { return true; }

	///--- 効果フック（必要なものだけ override） ---///

	// ダメージ（毒沼・ダメージ床など）
	virtual void ApplyAreaDamage([[maybe_unused]] float amount) {}

	// 回復（回復エリア）
	virtual void ApplyAreaHeal([[maybe_unused]] float amount) {}

	// 状態の付与/解除（バフ・デバフ。Enter で true / Exit で false を想定）
	virtual void SetAreaStatus([[maybe_unused]] const std::string& statusId,
		[[maybe_unused]] bool enable) {}
};
