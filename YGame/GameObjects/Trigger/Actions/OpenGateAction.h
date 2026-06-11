#pragma once

#include "../TriggerAction.h"

#include <functional>
#include <string>

// ============================================================
// OpenGateAction
//   指定の敵グループが撃破された通知を受け取ると、
//   名前で参照したターゲット PlacedObject の状態を「閉→開」に切り替える。
//
//   発火源は衝突ではなく外部シグナル (NotifyEnemyDefeated) のため、
//   Update 主導で Phase 遷移を行う。
//
//   開放時の処理:
//     1. ターゲット PlacedObject のコライダーを無効化
//     2. (任意) 開放アニメーションへ切り替え
//     3. onGateOpened_ コールバック発火 (FieldScene 側で RebakeNavGrid を呼ぶ用途)
// ============================================================
class OpenGateAction : public TriggerAction {
public:
	enum class Phase {
		Waiting,    // 撃破通知待ち
		Opening,    // 開放処理中 (アニメ再生中の余地)
		Opened,     // 完了
	};

	// 必須: 対象オブジェクトの nameTag と、条件となる敵グループ名
	OpenGateAction(std::string targetName, std::string requiredGroup);

	// 任意: 開放時に切り替えるアニメーションクリップ
	void SetOpenAnimation(const std::string& file, const std::string& clipName);

	// 任意: 開放が完了した瞬間に呼ばれるコールバック (RebakeNavGrid 用)
	void SetOnGateOpened(std::function<void()> cb) { onGateOpened_ = std::move(cb); }

	// FieldScene 側から FieldEnemyManager の撃破コールバック経由で叩く
	void NotifyEnemyDefeated(const std::string& group);

	void Update(float deltaTime) override;

	Phase GetPhase() const { return phase_; }

private:
	void TryOpen();

private:
	std::string targetName_;
	std::string requiredGroup_;
	std::string openAnimFile_;
	std::string openAnimClip_;
	std::function<void()> onGateOpened_;

	Phase phase_ = Phase::Waiting;
	bool  defeatSignal_ = false;     // 条件成立フラグ (NotifyEnemyDefeated で立つ)
};
