#pragma once

// App
#include "GuardConfig.h"

// Engine
#include <functional>
#include <Loaders/Json/JsonManager.h>
// C++


// 前方宣言
class Player;
class PlayerShield;
class BaseCollider;

/// <summary>
/// プレイヤーのガード状態クラス
/// </summary>
class PlayerGuard
{
public:

	enum class GuardResult {
		GuardFail,      // ガード失敗（被弾）
		GuardSuccess,   // 通常ガード成功
		ParrySuccess    // パリィ成功
	};

	///************************* 基本関数 *************************///
	PlayerGuard(Player* owner);
	void InitJson(YoRigine::JsonManager* jsonManager);

	bool StartGuard();

	void Update(float deltaTime);
	void Reset();

	GuardResult OnHit(BaseCollider* other);
	void ShowDebugImGui();

	///************************* コールバック設定 *************************///
	enum class State { Idle, StartUp, Active, Recovery };
	void SetOnGuardSuccess(std::function<void()>  cb) { onGuardSuccess_ = std::move(cb); }
	void SetOnParrySuccess(std::function<void()>  cb) { onParrySuccess_ = std::move(cb); }
	void SetOnGuardFail(std::function<void()>  cb) { onGuardFail_ = std::move(cb); }
	using StateCallback = std::function<void(State /*from*/, State /*to*/)>;
	void SetStateChangeCallback(StateCallback cb) { onStateChanged_ = std::move(cb); }

	bool IsGuarding()   const { return state_ == State::StartUp || state_ == State::Active; }
	bool IsParryWindow()const {
		return state_ == State::Active &&
			frame_ >= gc_.parryStart && frame_ <= gc_.parryEnd;
	}

	///************************* アクセッサ *************************///

	State GetState()const { return state_; }
private:

	///************************* 内部処理関数 *************************///
	void ChangeState(State s);

private:

	///************************* メンバ変数 *************************///

	// ポインタ
	Player* owner_ = nullptr;
	PlayerShield* shield_ = nullptr;
	GuardConfig gc_;

	// 状態管理
	State state_ = State::Idle;
	float frame_ = 0.0f;
	float timer_ = 0.0f;

	// コールバック
	std::function<void()> onGuardSuccess_ = nullptr;
	std::function<void()> onParrySuccess_ = nullptr;
	std::function<void()> onGuardFail_ = nullptr;
	StateCallback onStateChanged_;
};

