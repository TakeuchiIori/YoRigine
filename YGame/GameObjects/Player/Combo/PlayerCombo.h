#pragma once

// C++
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>

// App
#include "ComboTypes.h"

// Engine
#include "Loaders/Json/JsonManager.h"

class Player;

// ============================================================
// プレイヤーコンボ管理クラス
// CCの消費・回復や、連続攻撃のデータ（ダメージ倍率や履歴）を管理する。
// ※アニメーションの進行や入力受付等のロジックは AttackingCombatState が担当する。
// ============================================================
class PlayerCombo {
public:
	// ============================================================
	// 初期化と更新処理
	// ============================================================
	PlayerCombo(Player* owner);
	~PlayerCombo() = default;

	void Update(float deltaTime);
	void InitJson(YoRigine::JsonManager* jsonManager);

	// ============================================================
	// アクション・コンボ実行インターフェース
	// ============================================================
	bool TryAttack(AttackType attackType);
	bool CanAttack([[maybe_unused]] AttackType attackType) const;

	void CancelCombo();
	void ResetCombo();
	void ForceEndCombo();
	void ReloadAttacks();

	// ステート側から「攻撃アニメーションが終了した」ことを受け取る通知関数
	void OnAttackFinished();

	// ============================================================
	// CC（チェインキャパシティ）管理
	// ============================================================
	int GetCurrentCC() const { return currentCC_; }
	int GetMaxCC() const { return ccConfig_.maxCC; }
	bool HasSufficientCC(int cost) const { return currentCC_ >= cost; }

	void ConsumeCC(int amount);
	void RecoverCC(int amount);

	void OnDodgeSuccess();
	void OnCounterHit();
	void OnHitStep(const Vector3& enemyPosition);

	// ============================================================
	// コンボ状態の取得
	// ============================================================
	ComboState GetCurrentState() const { return currentState_; }
	ComboState GetPreviousState() const { return previousState_; }
	bool StateChanged() const { return currentState_ != previousState_; }

	int GetComboCount() const { return static_cast<int>(comboChain_.size()); }
	int GetMaxComboCount() const { return config_.maxLength; }
	bool IsComboActive() const { return currentAttack_ != nullptr; }

	float GetComboDamageMultiplier() const { return comboDamageMultiplier_; }
	const AttackData* GetCurrentAttack() const { return currentAttack_; }
	const std::vector<AttackData>& GetComboChain() const { return comboChain_; }

	float GetComboTimer() const { return comboTimer_; }
	float GetStateTimer() const { return stateTimer_; }

	float GetCurrentDamage() const {
		if (currentAttack_) return currentAttack_->baseDamage * comboDamageMultiplier_;
		return 0.0f;
	}

	float GetCurrentKnockback() const {
		if (currentAttack_) return currentAttack_->knockback;
		return 0.0f;
	}

	float GetCurrentKnockbackDuration() const {
		if (currentAttack_) return currentAttack_->knockbackDuration;
		return 0.0f;
	}

	// ============================================================
	// デバッグ表示とヘルパー
	// ============================================================
	void ShowDebugImGui();
	const char* GetStateString(ComboState state) const;

	// ============================================================
	// コールバック設定（演出やUIとの連携用）
	// ============================================================
	void SetAttackStartCallback(std::function<void(const AttackData&)> callback) { onAttackStart_ = callback; }
	void SetAttackContinueCallback(std::function<void(const AttackData&)> callback) { onAttackContinue_ = callback; }
	void SetComboEndCallback(std::function<void(int)> callback) { onComboEnd_ = callback; }
	void SetComboResetCallback(std::function<void()> callback) { onComboReset_ = callback; }
	void SetCCChangeCallback(std::function<void(int, int)> callback) { onCCChanged_ = callback; }

	using SwordColliderCallback = std::function<void(bool isActive)>;
	void SetSwordColliderCallback(SwordColliderCallback cb) { onSwordColliderChanged_ = cb; }

private:
	// ============================================================
	// 内部処理
	// ============================================================
	void InitializeAttacks();
	void ExecuteAttack(const AttackData& attack);
	void TryAutoHoming(const AttackData& attack);

	void UpdateCC(float deltaTime);
	void UpdateComboTimer(float deltaTime);

	AttackData* FindBestAttack(AttackType type);
	float CalculateDamageMultiplier() const;
	bool IsChainPreferred(AttackType from, AttackType to) const;

private:
	// ============================================================
	// メンバ変数
	// ============================================================

	// ------------------------------------------------------------
	// システム・オーナー参照
	// ------------------------------------------------------------
	Player* owner_;                                         // このコンボシステムを所有するプレイヤー

	// ------------------------------------------------------------
	// 状態管理 (UIやPlayerCombatのCanMove用データとしてのみ機能)
	// ------------------------------------------------------------
	ComboState currentState_ = ComboState::Idle;            // 現在のコンボフェーズ（UI表示用）
	ComboState previousState_ = ComboState::Idle;           // 1フレーム前のコンボフェーズ
	float stateTimer_ = 0.0f;                               // UI表示用の経過時間タイマー
	float comboTimer_ = 0.0f;                               // コンボが途切れるまでの猶予を計測するタイマー

	// ------------------------------------------------------------
	// コンバットコスト (CC) 管理
	// ------------------------------------------------------------
	int currentCC_ = 5;                                     // 現在使用可能なCC残量
	float ccRegenTimer_ = 0.0f;                             // CCの自然回復が開始されるまでの待機タイマー
	CCConfig ccConfig_;                                     // CCの最大値や回復速度などの設定データ

	// ------------------------------------------------------------
	// コンボ実行・データ管理
	// ------------------------------------------------------------
	std::vector<AttackData> comboChain_;                    // 現在継続中のコンボで繰り出した攻撃の履歴
	AttackData* currentAttack_ = nullptr;                   // 現在実行している攻撃のデータへのポインタ
	float comboDamageMultiplier_ = 1.0f;                    // 連携数などに応じて計算された現在のダメージ倍率
	ComboConfig config_;                                    // コンボの最大段数や倍率減衰などの設定データ

	// ------------------------------------------------------------
	// 攻撃データベース
	// ------------------------------------------------------------
	std::unordered_map<AttackType, std::vector<AttackData>> attackDatabase_;

	// ------------------------------------------------------------
	// イベントコールバック群 (他システムへの通知用)
	// ------------------------------------------------------------
	std::function<void(const AttackData&)> onAttackStart_;      // コンボ初撃開始時
	std::function<void(const AttackData&)> onAttackContinue_;   // コンボ継続時
	std::function<void(int)> onComboEnd_;                       // コンボ正常終了時
	std::function<void()> onComboReset_;                        // コンボ途切れ・強制終了時
	std::function<void(int, int)> onCCChanged_;                 // CC増減時
	SwordColliderCallback onSwordColliderChanged_;              // 攻撃判定切り替え用
};