#pragma once

#include <unordered_map>
#include <vector>

class BaseEnemy;

/// <summary>
/// 同時に攻撃してよい敵の数を制限する仕組み（attack token 方式）。
///
/// これが無いと全員が独立に判断するため、隙を見つけた瞬間に全員が同時に
/// 突っ込んでくる。囲まれている緊張感ではなく、ただ避けようのない理不尽に
/// なってしまう。攻撃権を持つ敵だけが仕掛け、残りは周囲を回って待つことで
/// 「囲まれている」状態が成立する。
///
/// 攻撃権を返した敵には短いクールダウンを掛けて、同じ個体が攻撃権を
/// 占有し続けないようにしている。
/// </summary>
class AttackTokenPool {
public:
  // 現在アクティブなプール（BattleEnemyManager が Initialize / Finalize
  // で設定する）。 戦闘外や未設定のときは nullptr
  // を返し、その場合は制限なしとして扱う。
  static AttackTokenPool *GetCurrent() { return current_; }
  static void SetCurrent(AttackTokenPool *pool) { current_ = pool; }

  // クールダウンを進める（毎フレーム呼ぶ）
  void Update(float dt);

  /// <summary>
  /// 攻撃権を取得する。取れたら true。
  /// 既に持っている場合も true を返す（多重取得しない）。
  /// </summary>
  bool TryAcquire(const BaseEnemy *enemy);

  // 攻撃権を返す。持っていない場合は何もしない。
  void Release(const BaseEnemy *enemy);

  // 敵が削除されるときに呼ぶ。攻撃権とクールダウンの両方を破棄する。
  void Forget(const BaseEnemy *enemy);

  // 全リセット（戦闘終了時など）
  void Clear();

  bool Holds(const BaseEnemy *enemy) const;

  int GetHolderCount() const { return static_cast<int>(holders_.size()); }
  const std::vector<const BaseEnemy *> &GetHolders() const { return holders_; }

  // この敵が再取得できるようになるまでの残り秒数（0 なら取得可能）
  float GetCooldownRemaining(const BaseEnemy *enemy) const;

  ///************************* 設定 *************************///

  int GetMaxTokens() const { return maxTokens_; }
  void SetMaxTokens(int value) { maxTokens_ = value; }

  float GetReacquireCooldown() const { return reacquireCooldown_; }
  void SetReacquireCooldown(float value) { reacquireCooldown_ = value; }

  bool IsEnabled() const { return enabled_; }
  void SetEnabled(bool value) { enabled_ = value; }

  // 設定への参照（エディタ用）
  int *GetMaxTokensPtr() { return &maxTokens_; }
  float *GetReacquireCooldownPtr() { return &reacquireCooldown_; }
  bool *GetEnabledPtr() { return &enabled_; }

private:
  static AttackTokenPool *current_;

  // false にすると全員が自由に攻撃できる（従来挙動。A/B比較用）
  bool enabled_ = true;

  // 同時に攻撃できる敵の数
  int maxTokens_ = 1;

  // 攻撃権を返してから再取得できるようになるまでの秒数。
  // 0 にすると同じ個体が連続で攻撃権を取り続けることがある。
  float reacquireCooldown_ = 1.0f;

  // 現在攻撃権を持っている敵（非所有）
  std::vector<const BaseEnemy *> holders_;

  // 敵ごとの再取得クールダウン残り秒数
  std::unordered_map<const BaseEnemy *, float> cooldowns_;
};
