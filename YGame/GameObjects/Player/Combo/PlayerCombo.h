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

// 前方宣言
class Player;
/// <summary>
/// プレイヤーのコンボクラス
/// </summary>
class PlayerCombo {
public:
	///************************* 基本関数 *************************///
	PlayerCombo(Player* owner);
	~PlayerCombo() = default;

	void Update(float deltaTime);
	void InitJson(YoRigine::JsonManager* jsonManager);

	///************************* 実行関数 *************************///

	// 実際に攻撃を実行
	bool TryAttack(AttackType attackType);
	// 指定した攻撃が可能かチェック
	bool CanAttack([[maybe_unused]] AttackType attackType) const;
	// コンボキャンセル
	void CancelCombo();
	// コンボリセット
	void ResetCombo();
	// コンボを強制終了
	void ForceEndCombo();

	// 攻撃データ再読み込み
	void ReloadAttacks();

	///************************* CC管理 *************************///

	int GetCurrentCC() const { return currentCC_; }
	int GetMaxCC() const { return ccConfig_.maxCC; }

	// 指定したCC消費が可能かチェック
	bool HasSufficientCC(int cost) const { return currentCC_ >= cost; }

	// CC消費
	void ConsumeCC(int amount);

	// CC回復
	void RecoverCC(int amount);

	// 回避成功時のCC回復処理
	void OnDodgeSuccess();

	// カウンター成功時のCC回復処理
	void OnCounterHit();

	// 攻撃がヒットした瞬間にステップを発火する（BattleEnemy側から呼び出す）
	void OnHitStep(const Vector3& enemyPosition);

	///************************* コンボ情報取得 *************************///

	// 現在の状態取得
	ComboState GetCurrentState() const { return currentState_; }

	// 前の状態取得
	ComboState GetPreviousState() const { return previousState_; }

	// 状態が変化したかチェック
	bool StateChanged() const { return currentState_ != previousState_; }

	// 現在のコンボ数取得
	int GetComboCount() const { return static_cast<int>(comboChain_.size()); }

	// 最大コンボ数取得
	int GetMaxComboCount() const { return config_.maxLength; }

	// コンボがアクティブかチェック
	bool IsComboActive() const { return currentState_ != ComboState::Idle; }

	// 現在のダメージ倍率取得
	float GetComboDamageMultiplier() const;

	// 現在実行中の攻撃データ取得
	const AttackData* GetCurrentAttack() const { return currentAttack_; }

	// コンボチェーン取得
	const std::vector<AttackData>& GetComboChain() const { return comboChain_; }

	// コンボタイマー取得
	float GetComboTimer() const { return comboTimer_; }

	// 状態タイマー取得
	float GetStateTimer() const { return stateTimer_; }

	///************************* デバッグ機能 *************************///

	void ShowDebugImGui();
	const char* GetStateString(ComboState state) const;

	///************************* コールバック設定 *************************///

	// 攻撃開始時
	void SetAttackStartCallback(std::function<void(const AttackData&)> callback) {
		onAttackStart_ = callback;
	}

	// コンボ継続時
	void SetAttackContinueCallback(std::function<void(const AttackData&)> callback) {
		onAttackContinue_ = callback;
	}

	// コンボ終了時
	void SetComboEndCallback(std::function<void(int)> callback) {
		onComboEnd_ = callback;
	}

	// コンボリセット時
	void SetComboResetCallback(std::function<void()> callback) {
		onComboReset_ = callback;
	}

	// CC変化時
	void SetCCChangeCallback(std::function<void(int, int)> callback) {
		onCCChanged_ = callback;
	}

	///************************* アクセッサ *************************///

	// 現在の攻撃ダメージ値取得
	float GetCurrentDamage() const {
		if (currentAttack_) {
			return currentAttack_->baseDamage * comboDamageMultiplier_;
		}
		return 0.0f;
	}

	// 現在の攻撃ノックバック値取得
	float GetCurrentKnockback() const {
		if (currentAttack_) {
			return currentAttack_->knockback;
		}
		return 0.0f;
	}

	// 現在の攻撃ノックバック値取得
	float GetCurrentKnockbackDuration() const {
		if (currentAttack_) {
			return currentAttack_->knockbackDuration;
		}
		return 0.0f;
	}

	// 現在の攻撃範囲取得
	Vector3 GetCurrentAttackRange() const {
		if (currentAttack_) {
			return currentAttack_->attackRange;
		}
		return Vector3(0.0f, 0.0f, 0.0f);
	}


private:
	///************************* 内部処理関数 *************************///

	// 攻撃データ初期化
	void InitializeAttacks();

	void ChangeState(ComboState newState);
	void EnterState(ComboState newState);
	void ExitState(ComboState oldState);
	// 攻撃実行処理
	void ExecuteAttack(const AttackData& attack);

	void UpdateCC(float deltaTime);
	void UpdateComboTimer(float deltaTime);

	// 最適な攻撃を検索
	AttackData* FindBestAttack(AttackType type);

	// ダメージ倍率計算
	float CalculateDamageMultiplier() const;

	// 推奨チェーンかどうかチェック
	bool IsChainPreferred(AttackType from, AttackType to) const;

	///************************* 状態別更新関数 *************************///

	void UpdateAttacking();
	void UpdateCanContinue();
	void UpdateRecovery();

private:
	///************************* メンバ変数 *************************///

	Player* owner_;
	std::unique_ptr<YoRigine::JsonManager> attackJson_;

	// 状態管理
	ComboState currentState_ = ComboState::Idle;    // 現在の状態
	ComboState previousState_ = ComboState::Idle;   // 前の状態
	float stateTimer_ = 0.0f;                       // 現在状態での経過時間
	float comboTimer_ = 0.0f;                       // コンボ開始からの経過時間

	// CC管理
	int currentCC_ = 5;                             // 現在のCC値
	float ccRegenTimer_ = 0.0f;                     // CC回復タイマー
	CCConfig ccConfig_;                             // CC設定

	// コンボ管理
	std::vector<AttackData> comboChain_;            // 実行済みコンボチェーン
	AttackData* currentAttack_ = nullptr;           // 現在実行中の攻撃
	float comboDamageMultiplier_ = 1.0f;            // 現在のダメージ倍率
	ComboConfig config_;                            // コンボ設定

	// 攻撃データベース
	std::unordered_map<AttackType, std::vector<AttackData>> attackDatabase_;

	// コールバック関数
	std::function<void(const AttackData&)> onAttackStart_;      // 攻撃開始時
	std::function<void(const AttackData&)> onAttackContinue_;   // コンボ継続時
	std::function<void(int)> onComboEnd_;                       // コンボ終了時
	std::function<void()> onComboReset_;                        // コンボリセット時
	std::function<void(int, int)> onCCChanged_;                 // CC変化時
};