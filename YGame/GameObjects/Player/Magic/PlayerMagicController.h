#pragma once

#include "MagicActionEditor.h"
#include "MagicAttackFactory.h"
#include "MagicActionRunner.h"

#include <memory>
#include <vector>

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
	void DrawCollision();

	bool TryCast(PlayerMagicSlot slot);
	void HandleSlotInput(PlayerMagicSlot slot, bool triggered, bool held);

#ifdef USE_IMGUI
	void ShowDebugImGui();
	void DrawEditorWindow();
#endif

private:
	bool CanCast() const;
	bool BeginCast(const MagicActionData& action);
	void SpawnAttackInstance(const MagicActionData& action);
	int SlotIndex(PlayerMagicSlot slot) const;
	void AdvanceChain(PlayerMagicSlot slot, const MagicActionData& action);
	void ResetChain(PlayerMagicSlot slot);

private:
	Player* owner_ = nullptr;
	MagicActionRunner runner_;
	MagicActionEditor editor_;
	std::vector<std::unique_ptr<MagicAttackInstance>> activeAttacks_;
	MagicActionData pendingChargeAction_;
	bool hasPendingChargeAction_ = false;
	bool previousHeld_[3] = {};
	int comboIndices_[3] = {};
	float comboTimers_[3] = {};
};
