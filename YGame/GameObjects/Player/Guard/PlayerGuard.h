#pragma once

// App
#include "GuardConfig.h"

// Engine
#include <functional>
#include <Loaders/Json/JsonManager.h>

class Player;
class PlayerShield;
class BaseCollider;

// ============================================================
// プレイヤーのガード状態管理クラス
// ガードの発生・パリィ受付・硬直フレーム等の監視を行い、攻撃ヒット時の防御判定を行う
// ============================================================
class PlayerGuard
{
public:

	// ガード判定の結果
	enum class GuardResult {
		GuardFail,      // ガード失敗（ダメージを受ける）
		GuardSuccess,   // 通常ガード成功（ダメージ軽減または無効）
		ParrySuccess    // パリィ成功（無敵化・カウンター等のボーナス）
	};

	// ガードの進行フェーズ
	enum class State {
		Idle,           // 待機中
		StartUp,        // ガード発生前
		Active,         // ガード有効期間（パリィ判定も含む）
		Recovery        // ガード解除時の硬直
	};

	// ============================================================
	// 初期化と更新処理
	// ============================================================
	PlayerGuard(Player* owner);
	void InitJson(YoRigine::JsonManager* jsonManager);

	bool StartGuard();
	void Update(float deltaTime);
	void Reset();

	// ============================================================
	// 判定とデバッグ
	// ============================================================
	GuardResult OnHit(BaseCollider* other);
	void ShowDebugImGui();

	// ============================================================
	// コールバック設定・アクセッサ
	// ============================================================
	void SetOnGuardSuccess(std::function<void()> cb) { onGuardSuccess_ = std::move(cb); }
	void SetOnParrySuccess(std::function<void()> cb) { onParrySuccess_ = std::move(cb); }
	void SetOnGuardFail(std::function<void()> cb) { onGuardFail_ = std::move(cb); }

	using StateCallback = std::function<void(State /*from*/, State /*to*/)>;
	void SetStateChangeCallback(StateCallback cb) { onStateChanged_ = std::move(cb); }

	bool IsGuarding() const { return state_ == State::StartUp || state_ == State::Active; }
	bool IsParryWindow() const {
		return state_ == State::Active &&
			frame_ >= gc_.parryStart && frame_ <= gc_.parryEnd;
	}

	State GetState() const { return state_; }

private:
	// ============================================================
	// 内部処理
	// ============================================================
	void ChangeState(State s);

private:
	// ============================================================
	// メンバ変数
	// ============================================================

	// ------------------------------------------------------------
	// システム連携・オーナー参照
	// ------------------------------------------------------------
	Player* owner_ = nullptr;                         // このガードシステムを所持するプレイヤーインスタンス
	PlayerShield* shield_ = nullptr;                  // ガードに用いる盾（シールド）オブジェクトへの参照
	GuardConfig gc_;                                  // ガード時のフレーム設定を格納するコンフィグデータ

	// ------------------------------------------------------------
	// 状態管理タイマー
	// ------------------------------------------------------------
	State state_ = State::Idle;                       // ガードの現在の進行状態
	float frame_ = 0.0f;                              // 現在のステートにおける経過フレーム数
	float timer_ = 0.0f;                              // （現在未使用）時間計測用の予備

	// ------------------------------------------------------------
	// コールバック・イベント通知
	// ------------------------------------------------------------
	std::function<void()> onGuardSuccess_ = nullptr;  // 通常のガードが成功したときに呼ばれる
	std::function<void()> onParrySuccess_ = nullptr;  // ジャストガード（パリィ）が成功したときに呼ばれる
	std::function<void()> onGuardFail_ = nullptr;     // ガード判定が発生していない等で防御に失敗したときに呼ばれる
	StateCallback onStateChanged_;                    // ガードのフェーズ（StartUpやActive等）が切り替わったときに呼ばれる
};