#pragma once

#include <memory>
#include <unordered_map>
#include <functional>
#include <type_traits>

template<typename StateEnum>
class IState;

template<typename StateEnum>
class StateMachine;

// ============================================================
// 状態の基底インターフェース
// ============================================================
template<typename StateEnum>
class IState {
public:
	virtual ~IState() = default;

	virtual void OnEnter() {}
	virtual void OnExit() {}
	virtual void Update(float deltaTime) = 0;

	virtual StateEnum GetStateType() const = 0;

protected:
	template<typename T>
	friend class StateMachine;

	StateMachine<StateEnum>* machine_ = nullptr;
};

// ============================================================
// 汎用StateMachine
// ============================================================
template<typename StateEnum>
class StateMachine {
	static_assert(std::is_enum_v<StateEnum>, "StateEnum must be an enum type");

public:
	StateMachine() = default;
	~StateMachine() = default;

	// ============================================================
	// 状態の登録と初期化
	// ============================================================
	template<typename StateClass, typename... Args>
	void RegisterState(StateEnum stateType, Args&&... args) {
		auto state = std::make_unique<StateClass>(std::forward<Args>(args)...);
		state->machine_ = this;
		states_[stateType] = std::move(state);
	}

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

	// ============================================================
	// 状態遷移と更新
	// ============================================================
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

		if (onStateChanged_) {
			onStateChanged_(previousStateType_, currentStateType_);
		}
	}

	void Update(float deltaTime) {
		if (currentState_) {
			currentState_->Update(deltaTime);
		}
	}

	// ============================================================
	// アクセッサ・コールバック
	// ============================================================
	StateEnum GetCurrentState() const { return currentStateType_; }
	StateEnum GetPreviousState() const { return previousStateType_; }
	bool StateChanged() const { return currentStateType_ != previousStateType_; }

	void SetStateChangeCallback(std::function<void(StateEnum, StateEnum)> callback) {
		onStateChanged_ = callback;
	}

	template<typename OwnerType>
	void SetOwner(OwnerType* owner) {
		owner_ = static_cast<void*>(owner);
	}

	template<typename OwnerType>
	OwnerType* GetOwner() const {
		return static_cast<OwnerType*>(owner_);
	}

private:
	// ============================================================
	// メンバ変数
	// ============================================================
	std::unordered_map<StateEnum, std::unique_ptr<IState<StateEnum>>> states_;
	IState<StateEnum>* currentState_ = nullptr;
	IState<StateEnum>* previousState_ = nullptr;
	StateEnum currentStateType_;
	StateEnum previousStateType_;

	void* owner_ = nullptr;
	std::function<void(StateEnum, StateEnum)> onStateChanged_;
};