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

	// 実際に接触ダメージを与えられる攻撃判定時間か。
	// 予備動作やチャージを持たない既存攻撃は IsAttacking() と同じ扱いにする。
	virtual bool IsContactDamageActive() const { return IsAttacking(); }

	// 同じ番号の攻撃判定時間では1回だけ命中させる。コンボは段ごとに番号を変える。
	virtual int GetContactDamageWindow() const { return IsContactDamageActive() ? 0 : -1; }

	// 被弾しても現在状態を維持するか。ダウンなどの反撃チャンスを
	// 通常のヒット状態で上書きしたくない場合に true を返す。
	virtual bool KeepsStateWhenDamaged() const { return false; }
};
