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

	// 攻撃実行中の状態か（攻撃State側で true を返す）。
	// プレイヤー本体への接触ダメージはこのフラグでゲートする。
	virtual bool IsAttacking() const { return false; }
};