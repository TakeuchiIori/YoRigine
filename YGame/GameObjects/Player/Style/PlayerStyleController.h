#pragma once

#include "PlayerStyle.h"

// ============================================================
// プレイヤースタイル管理
// 入力や攻撃実行は持たず、「今どの操作体系か」だけを責務にする。
// ============================================================
class PlayerStyleController {
public:
	void Reset();
	void Toggle();
	void SetStyle(PlayerStyle style);
	PlayerStyle GetStyle() const { return currentStyle_; }
	bool IsSword() const { return currentStyle_ == PlayerStyle::Sword; }
	bool IsMagic() const { return currentStyle_ == PlayerStyle::Magic; }

private:
	PlayerStyle currentStyle_ = PlayerStyle::Sword;
};
