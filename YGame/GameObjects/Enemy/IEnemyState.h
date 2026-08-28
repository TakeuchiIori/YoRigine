#pragma once

template <typename T>
/// <summary>
/// 状態パターンのインターフェース
/// </summary>
class IEnemyState {
public:
  virtual ~IEnemyState() = default;
  virtual void Enter(T &enemy) = 0;
  virtual void Update(T &enemy, float dt) = 0;
  virtual void Exit(T &enemy) = 0;

  // デバッグ表示用の状態名。エディタの状態モニタと遷移ログで使う。
  // これが無いと「今どの状態にいるか」を外から知る手段が一切ない。
  virtual const char *GetName() const { return "Unknown"; }

  // 攻撃実行中の状態か（攻撃State側で true を返す）。
  // プレイヤー本体への接触ダメージはこのフラグでゲートする。
  virtual bool IsAttacking() const { return false; }

  // 実際に接触ダメージを与えられる攻撃判定時間か。
  // 予備動作やチャージを持たない既存攻撃は IsAttacking() と同じ扱いにする。
  virtual bool IsContactDamageActive() const { return IsAttacking(); }

  // 同じ番号の攻撃判定時間では1回だけ命中させる。コンボは段ごとに番号を変える。
  virtual int GetContactDamageWindow() const {
    return IsContactDamageActive() ? 0 : -1;
  }

  // 被弾しても現在状態を維持するか。ダウンなどの反撃チャンスを
  // 通常のヒット状態で上書きしたくない場合に true を返す。
  virtual bool KeepsStateWhenDamaged() const { return false; }

  // 盾で受け止めるとダウンする攻撃か。
  // 以前は BattleEnemy 側が dynamic_cast で具象型を見ていたため、
  // 盾で止められるのは突進だけで、攻撃を増やすたびに判定を書き足す必要があった。
  // 各攻撃State が自分の攻撃データから読んで返す。
  virtual bool CanBeParried() const { return false; }
};
