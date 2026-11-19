#pragma once
#include "../Attack/Runtime/AttackPlayer.h"
#include "../BattleEnemy/BattleEnemyBase.h"
#include "Player/StateMachine.h"
#include "UI/HealthBar/EnemyHealthBarUI.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace YoRigine {
class Line;
}

///************************* ボスの状態 *************************///
// 攻撃の中身はデータ（EnemyAttack）が決めるので、攻撃が何種類あっても
// 状態は Attack の1つで済む。だから状態の集合が固定され、enum で管理できる。
enum class BossState {
  Intro,      // 登場の咆哮。無敵
  Idle,       // 次の行動を考える間
  Chase,      // 攻撃が届く距離まで詰める
  Attack,     // データで定義された攻撃を1つ再生する
  Stagger,    // 怯み値が溜まって崩れた／パリィされた
  PhaseShift, // HP の節目を越えた演出。無敵
  Dead,       // 死亡モーション → ディゾルブ
};

const char *BossStateToString(BossState state);

///************************* ボスの調整値 *************************///
// 敵データ表(enemy_data.json)ではなく JsonManager で個体ファイルに持つ。
// 雑魚と共通の項目（HP・攻撃力・移動速度・使う技）は敵データ表、
// ボスにしかない項目（クリップ名・怯み・フェーズ）はこちら、と分けている。
struct BossParams {
  // ── クリップ名 ──
  std::string idleClip = "Idle";
  std::string walkClip = "Walk";
  std::string runClip = "Run";
  std::string introClip = "Roaring";
  std::string phaseShiftClip = "PowerUp";
  std::string deathClip = "Die";

  // ルートモーションを取り出すジョイント（攻撃アニメの前進をボス本体の移動にする）
  std::string rootJoint = "Hips";

  // ── 見た目 ──
  // モデル自体の大きさの補正。エンカウントのスケール（フィールド敵から引き継ぐ値）に掛ける。
  // 攻撃判定の半径はこの倍率では大きくならない（メートルで直接作る）。
  float modelScale = 10.0f;

  // ── 移動 ──
  float turnSpeed = 4.0f;          // 旋回速度 (rad/s)
  float runDistance = 9.0f;        // これより遠いと走る
  float runSpeedMultiplier = 1.8f; // 走る時の速度倍率（moveSpeed に掛ける）

  // ── 思考の間 ──
  float thinkTimeMin = 0.5f; // 攻撃後に次の行動を決めるまでの最短秒数
  float thinkTimeMax = 1.2f; // 同・最長
  float chaseGiveUpTime =
      4.0f; // 追いかけても技が出せない時に一旦考え直すまでの秒数

  // ── 怯み ──
  // 雑魚のように毎ヒットで怯むとボスにならない。
  // 受けたダメージを怯み値として溜め、上限を超えた時だけ崩れる。
  float poiseMax = 80.0f;
  float poiseRecoverPerSec = 8.0f;
  float staggerDuration = 1.8f;
  float parryStaggerDuration = 2.5f; // パリィで崩された時はさらに長く

  // ── フェーズ ──
  // HP 割合がこれを下回った瞬間に PhaseShift を挟む。0 で無効。
  // フェーズごとの技の出し分けは、攻撃データの selfHpBelow で行う。
  float phase2HpRatio = 0.6f;
  float phase3HpRatio = 0.3f;

  // ── 登場 ──
  bool playIntro = true;

  // ── カメラ ──
  // ボス戦カメラが注視する胸のジョイントと、身長を測る頭のジョイント
  std::string cameraFocusJoint = "Spine02";
  std::string cameraTopJoint = "Head";

  // ── 当たり判定（プレイヤーを縦長のカプセルとみなす）──
  float playerHitRadius = 0.5f;
  float playerHitHeight = 1.7f;
};

///************************* ボス本体 *************************///
// 骨格アニメで攻撃する敵。
//
// 雑魚(BattleEnemy)との違い:
//   ・攻撃はアニメ＋ジョイント追従の判定（EnemyAttack の animation / joint）
//   ・毎ヒットでは怯まない（怯み値）
//   ・HP の節目でフェーズが変わる
//   ・攻撃権（AttackTokenPool）を使わない。取り巻きに順番を譲るボスはいない
//
// 同じもの: 配置・HP・被弾の受け口・攻撃データ・攻撃エディタのプレビュー。
class BossEnemy : public BattleEnemyBase {
public:
  BossEnemy();
  ~BossEnemy() override;

  ///************************* BattleEnemyBase *************************///

  void Initialize(YoRigine::Camera *camera) override;
  void InitializeBattleData(const BattleEnemyData &data, Vector3 position,
                            const Vector3 &scale) override;
  void InitCollision() override;
  void InitJson() override;
  void Update() override;
  void Draw() override;
  void DrawShadow() override;
  void DrawUI() override;
  void DrawCollision() override;
  const char *GetStateName() const override;

  ///************************* 当たり判定 *************************///

  void OnEnterCollision(BaseCollider *self, BaseCollider *other) override;
  void OnCollision(BaseCollider *self, BaseCollider *other) override;
  void OnDamaged(const DamageInfo &info) override;

  ///************************* 状態から呼ぶ *************************///

  void ChangeState(BossState state) { fsm_.ChangeState(state); }
  BossState GetState() const { return fsm_.GetCurrentState(); }

  const BossParams &GetParams() const { return params_; }

  // 使える技を1つ抽選する。クールダウン中の技は除く。無ければ nullptr
  const EnemyAttack *PickAttack() const;

  // 次に Attack 状態へ入った時に出す技を予約する
  void SetPendingAttack(const EnemyAttack *attack) { pendingAttack_ = attack; }
  const EnemyAttack *TakePendingAttack();

  // 攻撃が終わった（中断も含む）。クールダウンを開始する
  void OnAttackFinished(const EnemyAttack *attack);

  /// <summary>
  /// 今再生中の攻撃を知らせる（Attack 状態の開始時に渡し、終了時に nullptr）。
  /// 当たり判定はボス本体の Update 末尾、骨格アニメを更新した後に調べる。
  /// 状態の Update 内で調べると、腕の位置が1フレーム前のものになるため。
  /// </summary>
  void SetActiveAttack(AttackPlayer *player, const EnemyAttack *attack);

  /// <summary>
  /// ボス戦カメラ用の情報。アニメ中の実際の姿勢から求めるので、
  /// しゃがみ・跳躍・倒れ込みにもカメラが付いてくる。
  /// </summary>
  /// <param name="focus">注視点（胸）</param>
  /// <param name="height">足元から頭までの高さ</param>
  bool GetCameraFraming(Vector3 &focus, float &height);

  // 次に怯んだ時の長さ（パリィかどうかで変わる）
  float GetPendingStaggerDuration() const { return pendingStaggerDuration_; }

  // スーパーアーマー中か（Attack 状態が区間に合わせて切り替える）
  void SetSuperArmor(bool value) { superArmor_ = value; }

  /// <summary>
  /// 霧の中から実体化させる（ディゾルブを逆再生する）。生成直後に呼ぶ。
  /// 実体化の間も Intro（咆哮・無敵）は並行して進む。
  /// </summary>
  void BeginEmergence(float duration);
  bool IsEmerging() const { return emergenceTimer_ < emergenceDuration_; }

  ///************************* エディタ用 *************************///
  // ボス戦エディタから触る口。ゲーム中の判断には使わない。

  // AI（状態遷移）を止める。アニメ・判定の表示は動き続ける。
  // 攻撃プレビュー中に AI が別の行動を始めて姿勢を上書きしないようにする。
  void SetAIPaused(bool paused) { aiPaused_ = paused; }
  bool IsAIPaused() const { return aiPaused_; }

  BossParams &GetParamsMutable() { return params_; }
  int GetPhase() const { return phase_; }
  float GetPoise() const { return poise_; }
  float GetStandingHeight() const { return standingHeight_; }
  OBBCollider *GetOBBCollider() { return obbCollider_.get(); }

  // 編集した調整値（BossParams・コライダー・ディゾルブ）を個体ファイルへ保存する
  void SaveTuning();

  // モデル倍率を変えて即座に反映する（エンカウント由来のスケールは保つ）
  void ApplyModelScale(float modelScale);

  // デバッグ操作
  void ForceStagger();
  void ForcePhaseShift();
  // 指定の技を今すぐ出させる（AI停止中なら再開する。止めたままだと技が進まない）
  void ForceAttack(const EnemyAttack *attack);
  // 攻撃の姿勢から待機モーションへ戻す（プレビュー停止時）
  void ReturnToIdlePose() { PlayMotion(params_.idleClip, true); }

  // プレビュー中の攻撃。判定の球を表示するだけで、ダメージは与えない
  void SetPreviewAttack(const AttackPlayer *player) { previewAttackPlayer_ = player; }

  // モデルのジョイント名一覧（判定の付け先を選ぶプルダウン用）
  std::vector<std::string> GetJointNames();
  // モデルに入っているアニメーション名一覧（glTF から読む。初回だけファイルを開く）
  const std::vector<std::string> &GetAnimationClipNames();

  // ディゾルブの進行（Dead 状態から呼ぶ）
  void StartDissolve();
  void SetDissolveProgress(float t);
  float GetDissolveDuration() const { return dissolveDuration_; }

private:
  // 状態を登録して最初の状態へ入る
  void SetupStateMachine();

  // 霧からの実体化を進める
  void UpdateEmergence(float dt);

  // HP の節目を越えていたら PhaseShift へ。越えていれば true
  bool CheckPhaseTransition();

  // エンカウント由来のスケール（モデル倍率を除いた分）。攻撃判定の大きさに使う
  float EncounterScale() const {
    return std::max(wt_.scale_.y / std::max(params_.modelScale, 0.01f), 0.01f);
  }

  // 再生中の攻撃の当たり判定を調べてプレイヤーへダメージを通す
  void ProcessAttackHits();

  // ジョイント判定の中心（ジョイント指定が無い判定は本体基準）
  bool ResolveHitboxCenter(const AttackModifier &hitbox, Vector3 &out);

  // プレイヤーを縦カプセルとみなして球と重なっているか
  bool OverlapsPlayer(const Vector3 &center, float radius) const;

  // ダメージをプレイヤーへ通す。パリィされたら崩れる
  void DealDamageToPlayer(float multiplier, bool parriable);

private:
  StateMachine<BossState> fsm_;
  BossParams params_;

  // ── 攻撃 ──
  const EnemyAttack *pendingAttack_ = nullptr;
  const EnemyAttack *currentAttack_ = nullptr;
  AttackPlayer *activeAttackPlayer_ = nullptr; // Attack 状態の再生器（非所有）
  std::unordered_map<std::string, float> attackCooldowns_; // 技ID → 残り秒
  int lastDealtWindow_ = -1; // 同じ判定番号で二度当てない
  std::vector<const AttackModifier *> activeHitboxes_; // 毎フレーム使い回す
  std::vector<Vector3> debugHitboxCenters_;            // 判定の可視化用
  std::vector<float> debugHitboxRadii_;

  // ── 怯み ──
  float poise_ = 0.0f;
  bool superArmor_ = false;
  float pendingStaggerDuration_ = 0.0f;

  // ── エディタ ──
  bool aiPaused_ = false;
  const AttackPlayer *previewAttackPlayer_ = nullptr; // 判定表示だけ行うプレビュー（非所有）
  std::vector<std::string> clipNames_;                // glTF のアニメ名（遅延読み込み）
  bool clipNamesLoaded_ = false;

  // ── カメラ ──
  float standingHeight_ = 0.0f; // 立っている時の身長（攻撃中は測り直さない）

  // ── フェーズ ──
  int phase_ = 0; // 0 始まり。phase2HpRatio を割ったら 1

  // ── 出現演出（霧からの実体化）──
  float emergenceTimer_ = 0.0f;
  float emergenceDuration_ = 0.0f; // 0 なら出現演出なし
  Vector3 emergenceEdgeColor_ = {1.2f, 0.35f, 1.8f}; // 実体化の縁の発光色（紫）

  // ── 死亡演出 ──
  float dissolveDuration_ = 1.5f;
  float dissolveEdgeWidth_ = 0.08f;
  Vector3 dissolveEdgeColor_ = {1.8f, 0.4f, 0.15f};
  float dissolveNoiseScale_ = 6.0f;

  std::unique_ptr<EnemyHealthBarUI> healthBarUI_;
  std::unique_ptr<YoRigine::Line> debugLine_;
};
