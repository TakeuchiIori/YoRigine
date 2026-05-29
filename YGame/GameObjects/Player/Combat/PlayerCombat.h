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

// ============================================================
// プレイヤーの戦闘状態
// ============================================================
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

// ============================================================
// プレイヤー戦闘管理クラス
// 攻撃(Combo)や防御(Guard)の統合、戦闘状態(CombatState)の遷移を管理する
// ============================================================
class PlayerCombat {
public:
	// ============================================================
	// 初期化と更新処理
	// ============================================================
	PlayerCombat(Player* owner);
	~PlayerCombat() = default;

	void Update(float deltaTime);
	void Reset();

	// ============================================================
	// 各種アクションの実行要求
	// ============================================================
	bool TryAttack(AttackType type);
	bool TryDodge();
	bool TryGuard();
	bool TrySpecial();
	bool TryCancel();

	// ============================================================
	// 先行入力（入力バッファ）
	// 攻撃中に押された次の攻撃ボタンを記憶し、キャンセル可能フレームで消費する
	// ============================================================
	void BufferAttack(AttackType type);
	bool HasBufferedAttack() const { return hasBufferedInput_; }
	AttackType PopBufferedAttack();
	void ClearBufferedAttack() { hasBufferedInput_ = false; }
	void UpdateInputBuffer(float deltaTime);

	// ============================================================
	// 状態確認
	// ============================================================
	bool IsIdle() const;
	bool IsAttacking() const;
	bool IsDodging() const;
	bool IsStunned() const;
	bool IsDead() const;
	bool CanMove() const;
	bool CanAct() const;
	bool IsGuarding() const;
	bool IsHit() const;

	// 全身を占有するアクション中か（被弾・死亡・ガード・スタンなど）
	// Movementステートがこのフラグをチェックしてアニメーション再生を制御する
	bool IsFullBodyAction() const;

	void OnDodgeSuccess() { combo_->OnDodgeSuccess(); }
	void OnCounterHit() { combo_->OnCounterHit(); }

	bool StateChanged() const { return stateMachine_.StateChanged(); }
	void ChangeState(CombatState newState) { stateMachine_.ChangeState(newState); }

	// ============================================================
	// コールバック設定・通知
	// ============================================================
	void SetActionCallback(std::function<void(const std::string&)> callback) {
		onActionChanged_ = callback;
	}

	void NotifyAction(const std::string& action) {
		if (onActionChanged_) {
			onActionChanged_(action);
		}
	}

	// ============================================================
	// デバッグ表示
	// ============================================================
	void ShowDebugImGui();

	// ============================================================
	// アクセッサ
	// ============================================================
	PlayerCombo* GetPlayerCombo() const { return combo_.get(); }
	Player* GetOwner() const { return owner_; }

	void SetHitDirection(HitDirection dir) { lastHitDirection_ = dir; }
	HitDirection GetHitDirection() const { return lastHitDirection_; }

	int GetComboCount() const { return combo_->GetComboCount(); }
	float GetComboDamageMultiplier() const { return combo_->GetComboDamageMultiplier(); }
	ComboState GetComboState() const { return combo_->GetCurrentState(); }

	PlayerCombo* GetCombo() const { return combo_.get(); }
	PlayerGuard* GetGuard() const { return guard_.get(); }

	CombatState GetCurrentState() const { return stateMachine_.GetCurrentState(); }
	CombatState GetPreviousState() const { return stateMachine_.GetPreviousState(); }

	int GetCurrentCC() const { return combo_->GetCurrentCC(); }
	int GetMaxCC() const { return combo_->GetMaxCC(); }

private:
	// ============================================================
	// 内部処理
	// ============================================================
	void InitializeStateMachine();

private:
	// ============================================================
	// メンバ変数
	// ============================================================

	// ------------------------------------------------------------
	// システム連携・オーナー参照
	// ------------------------------------------------------------
	Player* owner_;                               // この戦闘システムを所有するプレイヤーインスタンス
	std::unique_ptr<PlayerCombo> combo_;          // 連続攻撃やCCの管理を行うコンボシステム
	std::unique_ptr<PlayerGuard> guard_;          // ガード判定やパリィの管理を行うガードシステム

	// ------------------------------------------------------------
	// 状態管理 (StateMachine)
	// ------------------------------------------------------------
	StateMachine<CombatState> stateMachine_;      // 現在の戦闘ステート（待機、攻撃中、被弾など）を管理

	// ------------------------------------------------------------
	// エフェクト・パーティクル
	// ------------------------------------------------------------
	std::unique_ptr<ParticleEmitter> guardEmitter_;   // ガード時のエフェクト生成
	std::unique_ptr<ParticleEmitter> parryEmitter_;   // パリィ成功時のエフェクト生成

	// ------------------------------------------------------------
	// コールバック・イベント通信
	// ------------------------------------------------------------
	std::function<void(const std::string&)> onActionChanged_; // アニメーション名などの行動変更を外部に通知するコールバック

	// ------------------------------------------------------------
	// 戦闘のパラメータ状態
	// ------------------------------------------------------------
	HitDirection lastHitDirection_ = HitDirection::Front;     // 直前に受けた攻撃の方向（ダメージモーションの分岐用）

	// ------------------------------------------------------------
	// 先行入力バッファ
	// ------------------------------------------------------------
	bool hasBufferedInput_ = false;                // 有効な先行入力が積まれているか
	AttackType bufferedAttackType_ = AttackType::A_Arte;
	float bufferedInputAge_ = 0.0f;                // バッファに積まれてからの経過秒
	float inputBufferLifetime_ = 0.4f;             // バッファの寿命（秒）。経過後は破棄
};