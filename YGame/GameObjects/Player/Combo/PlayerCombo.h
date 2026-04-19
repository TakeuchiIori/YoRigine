#pragma once

// C++
#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>

// App
#include "ComboTypes.h"

// Engine
#include "Loaders/Json/JsonManager.h"

class Player;

// ============================================================
// プレイヤーコンボ管理クラス
// CCの消費・回復や、連続攻撃の遷移状態、ダメージ倍率などを制御する
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
	bool IsComboActive() const { return currentState_ != ComboState::Idle; }

	float GetComboDamageMultiplier() const;
	const AttackData* GetCurrentAttack() const { return currentAttack_; }
	const std::vector<AttackData>& GetComboChain() const { return comboChain_; }

	float GetComboTimer() const { return comboTimer_; }
	float GetStateTimer() const { return stateTimer_; }

	float GetCurrentDamage() const {
		if (currentAttack_) {
			return currentAttack_->baseDamage * comboDamageMultiplier_;
		}
		return 0.0f;
	}

	float GetCurrentKnockback() const {
		if (currentAttack_) {
			return currentAttack_->knockback;
		}
		return 0.0f;
	}

	float GetCurrentKnockbackDuration() const {
		if (currentAttack_) {
			return currentAttack_->knockbackDuration;
		}
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
	void SetAttackStartCallback(std::function<void(const AttackData&)> callback) {
		onAttackStart_ = callback;
	}
	void SetAttackContinueCallback(std::function<void(const AttackData&)> callback) {
		onAttackContinue_ = callback;
	}
	void SetComboEndCallback(std::function<void(int)> callback) {
		onComboEnd_ = callback;
	}
	void SetComboResetCallback(std::function<void()> callback) {
		onComboReset_ = callback;
	}
	void SetCCChangeCallback(std::function<void(int, int)> callback) {
		onCCChanged_ = callback;
	}

	using SwordColliderCallback = std::function<void(bool isActive)>;
	void SetSwordColliderCallback(SwordColliderCallback cb) { onSwordColliderChanged_ = cb; }

private:
	// ============================================================
	// 内部状態遷移・処理
	// ============================================================
	void InitializeAttacks();
	void ChangeState(ComboState newState);
	void EnterState(ComboState newState);
	void ExitState(ComboState oldState);
	void ExecuteAttack(const AttackData& attack);

	void UpdateCC(float deltaTime);
	void UpdateComboTimer(float deltaTime);

	AttackData* FindBestAttack(AttackType type);
	float CalculateDamageMultiplier() const;
	bool IsChainPreferred(AttackType from, AttackType to) const;

	void UpdateAttacking();
	void UpdateCanContinue();
	void UpdateRecovery();

private:
	// ============================================================
	// メンバ変数
	// ============================================================

	// ------------------------------------------------------------
	// システム・オーナー参照
	// ------------------------------------------------------------
	Player* owner_;                                         // このコンボシステムを所有するプレイヤー
	std::unique_ptr<YoRigine::JsonManager> attackJson_;     // 攻撃パラメータ等のJSON管理オブジェクト

	// ------------------------------------------------------------
	// 状態管理 (State Management)
	// ------------------------------------------------------------
	ComboState currentState_ = ComboState::Idle;            // 現在のコンボフェーズ（攻撃中、硬直中など）
	ComboState previousState_ = ComboState::Idle;           // 1フレーム前のコンボフェーズ
	float stateTimer_ = 0.0f;                               // 現在のフェーズに入ってからの経過時間
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
	// JSONから読み込んだ全攻撃データを、攻撃タイプ（A術、B術など）ごとに分類して保持するマップ
	std::unordered_map<AttackType, std::vector<AttackData>> attackDatabase_;

	// ------------------------------------------------------------
	// イベントコールバック群 (他システムへの通知用)
	// ------------------------------------------------------------
	std::function<void(const AttackData&)> onAttackStart_;      // コンボ初撃が開始されたときに呼ばれる
	std::function<void(const AttackData&)> onAttackContinue_;   // コンボが2段目以降に繋がったときに呼ばれる
	std::function<void(int)> onComboEnd_;                       // コンボが正常に終了したときに呼ばれる（引数は総ヒット数）
	std::function<void()> onComboReset_;                        // コンボが途切れたり強制終了されたときに呼ばれる
	std::function<void(int, int)> onCCChanged_;                 // CCが増減したときに呼ばれる（引数は 変更前CC, 変更後CC）
	SwordColliderCallback onSwordColliderChanged_;              // 攻撃判定（剣の当たり判定）の有効/無効を切り替えるためのコールバック
};