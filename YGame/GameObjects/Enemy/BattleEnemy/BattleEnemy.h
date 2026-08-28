#pragma once
#include "../BaseEnemy.h"
#include "../IEnemyState.h"
#include "BattleEnemyData.h"
#include "UI/HealthBar/EnemyHealthBarUI.h"

#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <vector>

class Player;

///************************* 戦闘用の敵クラス *************************///
class BattleEnemy : public BaseEnemy {
public:
  ///************************* 基本関数 *************************///

  // デストラクタ
  ~BattleEnemy();

  // 初期化処理
  void Initialize(YoRigine::Camera *camera) override;

  // 戦闘データをもとに初期化（scale はフィールド敵から引き継ぐ見た目スケール）
  void InitializeBattleData(const BattleEnemyData &data, Vector3 position,
                            const Vector3 &scale = Vector3(1.0f, 1.0f, 1.0f));

  // 当たり判定初期化
  void InitCollision() override;

  // JSON初期化
  void InitJson() override;

  // 更新処理
  void Update() override;

  // 描画処理
  void Draw() override;

  // プレイヤー敗北時の半透明フェード色更新（インスタンシング描画から呼ぶ）
  void ApplyDeathFade();

  // UI描画
  void DrawUI();

  // 影の処理
  void DrawShadow();

  // 当たり判定の可視化描画
  void DrawCollision() override;

  ///************************* 当たり判定 *************************///

  // 衝突開始時
  void OnEnterCollision(BaseCollider *self, BaseCollider *other) override;

  // 衝突中
  void OnCollision(BaseCollider *self, BaseCollider *other) override;

  // 衝突終了時
  void OnExitCollision(BaseCollider *self, BaseCollider *other) override;

  // 方向付き衝突（前・後など）
  void OnDirectionCollision(BaseCollider *self, BaseCollider *other,
                            HitDirection dir) override;

  // 被弾リアクション（ダメージが実際に通ったときだけ呼ばれる）
  void OnDamaged(const DamageInfo &info) override;

  // 方向付き衝突開始時
  void OnEnterDirectionCollision([[maybe_unused]] BaseCollider *self,
                                 BaseCollider *other,
                                 [[maybe_unused]] HitDirection dir);

  ///************************* 状態管理 *************************///

  // 状態を変更
  void ChangeState(std::unique_ptr<IEnemyState<BattleEnemy>> newState);

  // 現在の論理状態を取得
  BattleEnemyState GetState() const { return logicalState_; }

  // 現在の状態クラスを取得
  IEnemyState<BattleEnemy> *GetCurrentState() const {
    return currentState_.get();
  }

  // 現在の状態名（デバッグ表示用）
  const char *GetStateName() const {
    return currentState_ ? currentState_->GetName() : "None";
  }

  ///*************************
  /// 遷移ログ（デバッグ用）*************************///

  // 1回分の状態遷移の記録
  struct StateTransitionLog {
    std::string name; // 遷移先の状態名
    float duration;   // 直前の状態に留まっていた秒数
    float lifeTime;   // 敵が生成されてからの経過秒
  };

  const std::vector<StateTransitionLog> &GetTransitionLog() const {
    return transitionLog_;
  }
  void ClearTransitionLog() { transitionLog_.clear(); }

  // 現在の状態に留まっている秒数
  float GetTimeInCurrentState() const { return timeInCurrentState_; }

  ///************************* アクセッサ *************************///

  // 敵データを取得
  const BattleEnemyData &GetEnemyData() const { return enemyData_; }
  BattleEnemyData &GetEnemyData() { return enemyData_; }

  // 死亡演出（ディゾルブ）パラメータ
  float GetDissolveDuration() const { return dissolveDuration_; }

  // ヒットカウント関連
  int GetConsecutiveHitCount() const { return consecutiveHitCount_; }
  void ResetConsecutiveHitCount() {
    consecutiveHitCount_ = 0;
    hitCountResetTimer_ = 0.0f;
  }

  ///************************* 攻撃・エフェクト処理 *************************///

  // 通常攻撃を実行
  void PerformBasicAttack();
  void TryPerformContactAttack();

  // 死亡エフェクトを再生
  void PlayDeathEffect();

private:
  ///************************* メンバ変数 *************************///

  // 現在の状態クラス
  std::unique_ptr<IEnemyState<BattleEnemy>> currentState_;

  // 論理状態
  BattleEnemyState logicalState_ = BattleEnemyState::Idle;

  // 戦闘データ
  BattleEnemyData enemyData_;

  // 同じ攻撃判定時間で二重にダメージを与えないための記録
  int lastDealtContactDamageWindow_ = -1;

  // 状態遷移の履歴（エディタ表示用のリングバッファ）
  std::vector<StateTransitionLog> transitionLog_;
  size_t maxTransitionLog_ = 40;
  float timeInCurrentState_ = 0.0f;
  float lifeTime_ = 0.0f;

  // フェードスピード（プレイヤー敗北時の消失演出）
  float fadeSpeed_ = 3.0f;

  // 死亡演出（ディゾルブ）。エディタから JsonManager 経由で調整
  float dissolveDuration_ = 1.2f;   // 0→1 まで何秒かけて消えるか
  float dissolveEdgeWidth_ = 0.08f; // エッジ発光帯の幅
  Vector3 dissolveEdgeColor_ = {1.8f, 0.7f,
                                0.15f}; // エッジ発光色（オレンジ系）
  float dissolveNoiseScale_ = 6.0f;     // ノイズの空間スケール

  std::unique_ptr<EnemyHealthBarUI> healthBarUI_;

  // 連続ヒット管理（しきい値・リセット時間は enemyData_.attackParams.counter
  // から取得）
  int consecutiveHitCount_ = 0;
  float hitCountResetTimer_ = 0.0f;
};
