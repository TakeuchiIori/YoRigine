#pragma once

template<typename T>
/// <summary>
/// 状態パターンのインターフェース
/// </summary>
class IEnemyState {
public:
	virtual ~IEnemyState() = default;
	virtual void Enter(T& enemy) = 0;
	virtual void Update(T& enemy, float dt) = 0;
	virtual void Exit(T& enemy) = 0;
};