#pragma once

#include "MagicActionEditor.h"
#include "MagicActionRunner.h"

// ============================================================
// プレイヤー魔法コントローラー
// 入力から選ばれた魔法スロットを受け取り、実行可否と発火窓口だけを担当する。
// 個別魔法の効果はここへ直書きせず、将来の SpellAction / MagicLoadout 側へ逃がす。
// ============================================================
class PlayerMagicController {
public:
	explicit PlayerMagicController(Player* owner);
	~PlayerMagicController() = default;

	void Update(float deltaTime);
	void Reset();

	bool TryCast(PlayerMagicSlot slot);
	void HandleSlotInput(PlayerMagicSlot slot, bool triggered, bool held);

#ifdef USE_IMGUI
	void ShowDebugImGui();
#endif

private:
	bool CanCast() const;
	bool BeginCast(const MagicActionData& action);
	int SlotIndex(PlayerMagicSlot slot) const;

private:
	Player* owner_ = nullptr;
	MagicActionRunner runner_;
	MagicActionEditor editor_;
	bool previousHeld_[3] = {};
};
