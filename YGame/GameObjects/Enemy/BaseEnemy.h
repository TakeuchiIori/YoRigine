#pragma once
#include "Object3D/BaseObject.h"

#include "Particle/EffectHandle.h"
#include <Systems/Animation/ObjectAnimation.h>

#include <memory>
#include <string>

class Player;

///************************* ノックバックデータ *************************///
struct KnockbackData {
  bool isKnockingBack_ = false;
  Vector3 knockbackDirection_;
  float knockbackPower_ = 0.0f;
  float knockbackDuration_ = 0.0f;
  float knockbackTimer_ = 0.0f;
};

///************************* ダメージ情報 *************************///
// 「誰にどう殴られたか」をまとめて派生へ渡すための構造体。
// これがあると当たり判定コールバックは情報を集めるだけで済み、
// 状態遷移やリアクションの判断を OnDamaged 側へ寄せられる。
struct DamageInfo {
  int amount = 0; // 与ダメージ
  Vector3
      sourcePosition{}; // 攻撃してきた側の位置（ノックバック方向の算出に使う）
  float knockbackPower = 0.0f;
  float knockbackDuration = 0.0f;
};

///************************* 敵の共通基底クラス *************************///
// BattleEnemy / FieldEnemy / 今後の BossEnemy が共有する「敵なら必ず持つ」
// データと振る舞いだけを担当する。
//
// ここに置かないもの:
//   ・状態管理そのもの (IEnemyState / StateMachine)     …
//   派生ごとに状態集合が違う ・体力バーやアラートなどの UI … 派生固有
//   ・NavPathfinder / フォーメーション                   …
//   派生とマネージャの責務
class BaseEnemy : public YoRigine::BaseObject {
public:
  ~BaseEnemy() override = default;

  ///************************* 体力 *************************///

  // HPだけを減らす。無敵中・死亡後は無視される。
  // リアクション（のけぞり・状態遷移）は起こさないので、
  // デバッグ用の即死ボタンや、演出を挟まない魔法ダメージはこちらを使う。
  void TakeDamage(int damage);

  /// <summary>
  /// ダメージを与え、被弾リアクションまで走らせる。
  /// TakeDamage と分けているのは、既存の魔法ダメージが
  /// 「HPだけ減らしてのけぞらせない」挙動になっているため。
  /// </summary>
  void ApplyDamage(const DamageInfo &info);

  /// <summary>
  /// ダメージを受けた直後に呼ばれる。状態遷移やリアクションは派生が実装する。
  /// 無敵などで実際にHPが減らなかった場合は呼ばれない。
  /// </summary>
  virtual void OnDamaged([[maybe_unused]] const DamageInfo &info) {}

  // HPを回復する。死亡後は無視される。
  void Heal(int amount);

  int GetCurrentHP() const { return currentHp_; }
  int GetMaxHP() const { return maxHp_; }
  float GetHPRatio() const {
    return maxHp_ > 0
               ? static_cast<float>(currentHp_) / static_cast<float>(maxHp_)
               : 0.0f;
  }

  // 最大HPを設定し、現在HPを満タンにする（スポーン時に呼ぶ）
  void SetupHP(int maxHp) {
    maxHp_ = maxHp;
    currentHp_ = maxHp;
  }

  bool IsAlive() const { return isAlive_; }
  void SetIsAlive(bool v) { isAlive_ = v; }

  // 参照返しは State 側で `enemy.IsInvincible() = true;`
  // と書けるようにするため。
  bool &IsInvincible() { return isInvincible_; }
  bool IsInvincible() const { return isInvincible_; }
  bool &IsDamageBlinking() { return isDamageBlinking_; }
  bool IsDamageBlinking() const { return isDamageBlinking_; }

  ///************************* ターゲット *************************///

  void SetPlayer(Player *player) { player_ = player; }
  Player *GetPlayer() const { return player_; }
  bool HasPlayer() const { return player_ != nullptr; }

  // プレイヤーの現在位置。未設定なら原点を返す。
  Vector3 GetPlayerPosition() const;

  // プレイヤーまでの距離。未設定なら 0 を返す。
  float GetDistanceToPlayer() const;

  // 最後に確認したプレイヤー位置（見失った後の索敵に使う）
  void SetLastKnownPlayerPosition(const Vector3 &pos) {
    lastKnownPlayerPosition_ = pos;
    hasValidTarget_ = true;
  }
  Vector3 GetLastKnownPlayerPosition() const {
    return lastKnownPlayerPosition_;
  }
  bool HasValidTarget() const { return hasValidTarget_; }

  ///************************* 状態タイマー *************************///

  void ResetStateTimer() { stateTimer_ = 0.0f; }
  void AddStateTimer(float dt) { stateTimer_ += dt; }
  float GetStateTimer() const { return stateTimer_; }

  ///************************* 行動制御 *************************///

  void SetCanAct(bool v) { canAct_ = v; }
  bool CanAct() const { return canAct_; }

  ///************************* 移動 *************************///

  void AddTranslate(const Vector3 &delta) { wt_.translate_ += delta; }

  // 移動目標地点。到達判定は GetArrivalThreshold() と比較する。
  void SetTargetPosition(const Vector3 &pos) {
    targetPosition_ = pos;
    hasTargetPosition_ = true;
  }
  Vector3 GetTargetPosition() const { return targetPosition_; }
  void SetHasTargetPosition(bool v) { hasTargetPosition_ = v; }
  bool HasTargetPosition() const { return hasTargetPosition_; }
  float GetArrivalThreshold() const { return arrivalThreshold_; }

  // 直前フレームからの移動量。コライダーへ渡す速度に使う。
  const Vector3 &GetVelocity() const { return currentVelocity_; }

  ///************************* 回転 *************************///

  void SetRotationY(float y) { wt_.rotate_.y = y; }
  float GetRotationY() const { return wt_.rotate_.y; }
  float &GetRotationX() { return wt_.rotate_.x; }
  float &GetRotationZ() { return wt_.rotate_.z; }

  /// <summary>
  /// 目標角度へ最短方向に補間回転する（毎フレーム呼ぶ）
  /// </summary>
  /// <param name="targetAngle">目標Y回転角（ラジアン）</param>
  /// <param name="speed">回転速度（rad/s）</param>
  void RotateTowards(float targetAngle, float speed, float dt);

  // プレイヤー方向へ補間回転する
  void RotateTowardsPlayer(float speed, float dt);

  // 指定方向へ補間回転する
  void RotateTowardsDirection(const Vector3 &direction, float speed, float dt);

  // 指定方向へ即座に向く（補間なし）
  void FaceDirection(const Vector3 &direction);

  // プレイヤー方向へ即座に向く（補間なし）
  void FacePlayer();

  ///************************* 見た目 *************************///

  void SetColor(const Vector4 &c) {
    if (obj_)
      obj_->SetMaterialColor(c);
  }

  // のけぞり回転を含んだ描画専用 Transform。コライダーは wt_ のまま傾けない。
  YoRigine::WorldTransform &GetVisualWT() { return visualWt_; }

  ObjectAnimation *GetAnimation() { return animation_.get(); }

  // 本体のコライダー。ロックオン対象の照合などに使う（所有権は渡さない）
  const BaseCollider *GetPrimaryCollider() const { return obbCollider_.get(); }

  ///************************* 被弾リアクション *************************///

  void StartKnockback(const Vector3 &direction, float power, float duration);
  void UpdateKnockback(float dt);
  const KnockbackData &GetKnockbackData() const { return knockbackData_; }

  // 攻撃を受けた方向に応じて見た目上のけぞらせる。
  // 角度と時間は敵データ側で持つので、開始時に渡してもらう。
  void StartDirectionalHitReaction(const Vector3 &direction, float angle,
                                   float duration);
  void UpdateDirectionalHitReaction(float dt);

  // ダメージ時の点滅更新（isDamageBlinking_ が true の間だけ動く）
  void UpdateBlinking(float dt);
  void SetBlinkSpeed(float speed) { blinkSpeed_ = speed; }

  ///************************* 状態VFX *************************///

  // 燃焼・感電などの付着VFX。Composite名をループ再生して自分に追従させ、
  // duration
  // 秒後（または明示停止時）に自動で止まる。再付着は時間をリフレッシュする。
  void AttachStatusVfx(const std::string &compositeName, float duration);
  void UpdateStatusVfx(float dt);
  void StopStatusVfx();

protected:
  ///************************* 派生から呼ぶ共通更新 *************************///

  // アニメーターを進め、スケール／カラーを本体へ反映する
  void UpdateAnimation(float dt);

  // ノックバック・のけぞり・状態VFX をまとめて更新する
  void UpdateReactions(float dt);

  // wt_ の内容にのけぞり回転を足して visualWt_ へ反映する
  void SyncVisualTransform();

  // 移動速度を previousPosition_ との差分から求める（wt_.UpdateMatrix
  // の直前に呼ぶ）
  void UpdateVelocity();

  // 現在位置を「前フレームの位置」として記録する（Update の先頭で呼ぶ）
  void MarkPreviousPosition() { previousPosition_ = wt_.translate_; }

protected:
  ///************************* メンバ変数 *************************///

  // ── 体力 ──
  int currentHp_ = 1;
  int maxHp_ = 1;
  bool isAlive_ = true;
  bool isInvincible_ = false;

  // ── ターゲット ──
  Player *player_ = nullptr;
  Vector3 lastKnownPlayerPosition_{};
  bool hasValidTarget_ = false;

  // ── 状態 ──
  float stateTimer_ = 0.0f;
  bool canAct_ = true;

  // ── 移動 ──
  Vector3 targetPosition_{};
  bool hasTargetPosition_ = false;
  float arrivalThreshold_ = 0.5f;
  Vector3 previousPosition_{};
  Vector3 currentVelocity_{};

  // ── 見た目 ──
  YoRigine::WorldTransform visualWt_;
  std::unique_ptr<ObjectAnimation> animation_;

  // ── ダメージ点滅 ──
  bool isDamageBlinking_ = false;
  float blinkTimer_ = 0.0f;
  float blinkSpeed_ = 50.0f;

  // ── ノックバック ──
  KnockbackData knockbackData_;

  // ── 攻撃方向に応じた見た目上ののけぞり ──
  Vector3 hitReactionRotation_{};
  Vector3 hitReactionTargetRotation_{};
  float hitReactionTimer_ = 0.0f;
  float hitReactionDuration_ = 0.22f;
  float hitReactionAngle_ = 0.20f;
  bool isHitReacting_ = false;

  // ── 状態VFX（燃焼など）。ループ再生ハンドルと残り時間 ──
  EffectHandle statusVfx_;
  float statusVfxTimer_ = 0.0f;
  std::string statusVfxName_;
};
