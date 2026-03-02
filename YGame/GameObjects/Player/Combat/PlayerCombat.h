#pragma once

// App
#include "../Combo/PlayerCombo.h"
#include "../Guard/PlayerGuard.h"
#include "../StateMachine.h"

// C++
#include <functional>
#include <memory>

// Engine
#include <Particle/ParticleEmitter.h>
#include "Collision/Core/CollisionDirection.h"

// 戦闘状態
enum class CombatState {
	Idle,           // 待機
	Attacking,      // 攻撃中
	Guarding,       // ガード中
	Dodging,        // 回避中
	Stunned,        // スタン中
	Dead,			// 死亡中
	Hit				// 被弾中
};

class Player;

/// <summary>
/// プレイヤーの戦闘管理クラス
/// </summary>
class PlayerCombat {
public:
	///************************* 基本的関数 *************************///
	PlayerCombat(Player* owner);
	~PlayerCombat() = default;

	void Update(float deltaTime);
	void Reset();

	///************************* 攻撃実行 *************************///
	bool TryAttack(AttackType type);
	bool TryDodge();
	bool TryGuard();
	bool TrySpecial();
	bool TryCancel();

	///************************* 状態確認 *************************///
	bool IsIdle() const;
	bool IsAttacking() const;
	bool IsDodging() const;
	bool IsStunned() const;
	bool IsDead() const;
	bool CanMove() const;
	bool CanAct() const;
	bool IsGuarding() const;
	bool IsHit() const;

	void OnDodgeSuccess() { combo_->OnDodgeSuccess(); }
	void OnCounterHit() { combo_->OnCounterHit(); }

	bool StateChanged() const { return stateMachine_.StateChanged(); }
	void ChangeState(CombatState newState) { stateMachine_.ChangeState(newState); }

	///************************* コールバック設定 *************************///
	
	// アクション変更コールバック設定
	void SetActionCallback(std::function<void(const std::string&)> callback) {
		onActionChanged_ = callback;
	}

	// Stateからアクションを通知
	void NotifyAction(const std::string& action) {
		if (onActionChanged_) {
			onActionChanged_(action);
		}
	}

	///************************* デバッグ *************************///
	void ShowDebugImGui();

	///************************* アクセッサ *************************///

	// コンボ・ガードシステム取得
	PlayerCombo* GetPlayerCombo() const { return combo_.get(); }
	Player* GetOwner() const { return owner_; }

	// 衝突方向の設定・取得
	void SetHitDirection(HitDirection dir) { lastHitDirection_ = dir; }
	HitDirection GetHitDirection() const { return lastHitDirection_; }

	// コンボ情報
	int GetComboCount() const { return combo_->GetComboCount(); }
	float GetComboDamageMultiplier() const { return combo_->GetComboDamageMultiplier(); }
	ComboState GetComboState() const { return combo_->GetCurrentState(); }

	// 状態クラスへアクセス
	PlayerCombo* GetCombo() const { return combo_.get(); }
	PlayerGuard* GetGuard() const { return guard_.get(); }

	// StateMachine関連
	CombatState GetCurrentState() const { return stateMachine_.GetCurrentState(); }
	CombatState GetPreviousState() const { return stateMachine_.GetPreviousState(); }

	// CC関連
	int GetCurrentCC() const { return combo_->GetCurrentCC(); }
	int GetMaxCC() const { return combo_->GetMaxCC(); }

private:
	///************************* 内部処理 *************************///
	void InitializeStateMachine();

private:
	///************************* メンバ変数 *************************///

	// ポインタ
	Player* owner_;
	std::unique_ptr<PlayerCombo> combo_;
	std::unique_ptr<PlayerGuard> guard_;

	// StateMachine
	StateMachine<CombatState> stateMachine_;

	std::unique_ptr<ParticleEmitter> guardEmitter_;
	std::unique_ptr<ParticleEmitter> parryEmitter_;

	// コールバック
	std::function<void(const std::string&)> onActionChanged_;

	// 最後に受けたヒットの方向
	HitDirection lastHitDirection_ = HitDirection::Front;

};