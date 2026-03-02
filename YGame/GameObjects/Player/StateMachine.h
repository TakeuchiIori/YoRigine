#pragma once

#include <memory>
#include <unordered_map>
#include <functional>
#include <type_traits>

// 前方宣言
template<typename StateEnum>
class IState;

template<typename StateEnum>
class StateMachine;

/// <summary>
/// 状態の基底インターフェース
/// </summary>
template<typename StateEnum>
class IState {
public:
	virtual ~IState() = default;

	// 状態に入った時
	virtual void OnEnter() {}

	// 状態を抜ける時
	virtual void OnExit() {}

	// 毎フレーム更新
	virtual void Update(float deltaTime) = 0;

	// 状態の種類を取得
	virtual StateEnum GetStateType() const = 0;

protected:
	// StateMachineへのアクセスを許可
	template<typename T>
	friend class StateMachine;

	StateMachine<StateEnum>* machine_ = nullptr;
};

/// <summary>
/// 汎用StateMachine
/// </summary>
template<typename StateEnum>
class StateMachine {
	static_assert(std::is_enum_v<StateEnum>, "StateEnum must be an enum type");

public:
	StateMachine() = default;
	~StateMachine() = default;

	/// <summary>
	/// 状態を登録
	/// </summary>
	template<typename StateClass, typename... Args>
	void RegisterState(StateEnum stateType, Args&&... args) {
		auto state = std::make_unique<StateClass>(std::forward<Args>(args)...);
		state->machine_ = this;
		states_[stateType] = std::move(state);
	}

	/// <summary>
	/// 初期状態を設定
	/// </summary>
	void SetInitialState(StateEnum stateType) {
		if (states_.find(stateType) == states_.end()) {
			return;
		}
		currentState_ = states_[stateType].get();
		previousState_ = currentState_;
		currentStateType_ = stateType;
		previousStateType_ = stateType;

		if (currentState_) {
			currentState_->OnEnter();
		}
	}

	/// <summary>
	/// 状態遷移
	/// </summary>
	void ChangeState(StateEnum newStateType) {
		if (states_.find(newStateType) == states_.end()) {
			return;
		}

		if (currentState_) {
			currentState_->OnExit();
		}

		previousState_ = currentState_;
		previousStateType_ = currentStateType_;

		currentState_ = states_[newStateType].get();
		currentStateType_ = newStateType;

		if (currentState_) {
			currentState_->OnEnter();
		}

		// 状態変更コールバック実行
		if (onStateChanged_) {
			onStateChanged_(previousStateType_, currentStateType_);
		}
	}

	/// <summary>
	/// 更新
	/// </summary>
	void Update(float deltaTime) {
		if (currentState_) {
			currentState_->Update(deltaTime);
		}
	}

	/// <summary>
	/// 現在の状態を取得
	/// </summary>
	StateEnum GetCurrentState() const { return currentStateType_; }

	/// <summary>
	/// 前の状態を取得
	/// </summary>
	StateEnum GetPreviousState() const { return previousStateType_; }

	/// <summary>
	/// 状態が変化したか
	/// </summary>
	bool StateChanged() const { return currentStateType_ != previousStateType_; }

	/// <summary>
	/// 状態変更時のコールバック設定（最小限に）
	/// </summary>
	void SetStateChangeCallback(std::function<void(StateEnum, StateEnum)> callback) {
		onStateChanged_ = callback;
	}

	/// <summary>
	/// Owner（プレイヤーなど）を設定
	/// </summary>
	template<typename OwnerType>
	void SetOwner(OwnerType* owner) {
		owner_ = static_cast<void*>(owner);
	}

	/// <summary>
	/// Ownerを取得
	/// </summary>
	template<typename OwnerType>
	OwnerType* GetOwner() const {
		return static_cast<OwnerType*>(owner_);
	}

private:
	std::unordered_map<StateEnum, std::unique_ptr<IState<StateEnum>>> states_;
	IState<StateEnum>* currentState_ = nullptr;
	IState<StateEnum>* previousState_ = nullptr;
	StateEnum currentStateType_;
	StateEnum previousStateType_;

	void* owner_ = nullptr;  // 型消去されたオーナーポインタ

	// 最小限のコールバック（アニメーション切り替えなど外部連携用）
	std::function<void(StateEnum, StateEnum)> onStateChanged_;
};
