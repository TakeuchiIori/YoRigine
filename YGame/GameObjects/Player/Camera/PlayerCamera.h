#pragma once
#include "AttackCameraComponent.h"

#ifdef USE_IMGUI
#include "AttackCameraEditor.h"
#endif

#include "Collision/Core/BaseCollider.h"
#include "Object3D/ObjectManager.h"
#include "Systems/Camera/Virtuals/FollowCamera/BattleStartCameraState.h"
#include "Systems/Camera/Virtuals/FollowCamera/FollowCamera.h"
#include "WorldTransform/WorldTransform.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/// <summary>
/// プレイヤー専用カメラ管理クラス
///
/// 保有コンポーネント:
///   FollowCamera*            … Engine 側の追従カメラ本体
///   CameraCollisionResolver  … FollowCamera 内部に組み込み済み
///   AttackCameraComponent    … 攻撃ごとのカメラワーク再生
///   lock-on ロジック         … FollowCamera から移設
///   BattleStart 演出         … FollowCamera から移設
///
/// Player は raw の FollowCamera* を直接保持せず、
/// PlayerCamera を通してカメラを操作する。
/// </summary>
class PlayerCamera {
public:
  // ============================================================
  // 基本関数
  // ============================================================

  /// @param followCamera  CameraDirector が所有する FollowCamera インスタンス
  /// @param playerWT      プレイヤーの WorldTransform（ロックオン基準位置用）
  void Initialize(FollowCamera *followCamera, const YoRigine::WorldTransform *playerWT);

  // スティック入力・ロックオン更新
  void UpdatePreDirector();

  // 攻撃カメラオフセットを sceneCamera に適用
  void ApplyPostDirector(YoRigine::Camera *sceneCamera, float dt);

  void DrawImGui();

  // ============================================================
  // 攻撃カメラワーク
  // ============================================================
  void PlayAttackCameraWork(const std::string &attackName);

  /// Anchor 注視（LookAtTarget::Anchor）や TargetRelative の基準に使う
  /// 任意のワールド座標（攻撃ヒット点など）を渡して再生する。
  void PlayAttackCameraWork(const std::string &attackName,
                            const Vector3 &anchor);

  void StopAttackCameraWork();
  bool IsAttackCameraPlaying() const { return attackCamera_.IsPlaying(); }

  // ============================================================
  // 戦闘開始演出
  // ============================================================
  void PlayBattleStart();
  bool IsInPerformance() const;

  // ============================================================
  // ロックオン
  // ============================================================
  BaseCollider *GetLockedTarget() const { return lockedTarget_; }
  bool IsLockOn() const { return isLockOn_; }

  // 敵更新後に呼ぶ。同フレームで削除されたコライダーへのダングリング参照を解消する。
  void ValidateLockOnTarget();

  // ============================================================
  // 戦闘モード
  //   true の間は RB/LB を「押しっぱなしで左右カメラ回転」に割り当てる。
  //   false（フィールド等）では従来どおり RB/LB は背後リセンター。
  //   GameScene が毎フレーム、現在サブシーンが Battle かで設定する。
  // ============================================================
  void SetBattleMode(bool b) {
    battleMode_ = b;
    // 戦闘中はアイドルオートリセンターを止める。別方向を向いているのに
    // 無操作で勝手に正面へ戻るとストレスなので、戦闘中は自動リセンターしない。
    if (followCamera_)
      followCamera_->SetIdleRecenterSuppressed(b);
  }
  bool IsBattleMode() const { return battleMode_; }

  // ============================================================
  // 脅威察知のシーン許可
  // フィールドでは背景・マップ見渡しを優先して OFF、バトル中だけ ON にする。
  // 各サブシーンの OnEnter から呼ぶ。
  // 同じフラグでアイドルオートリセンターも制御する：バトル中に無操作が
  // 続いてもカメラが勝手に背後へ回り込まないよう、ここで一時停止させる。
  // ============================================================
  void SetThreatAwarenessAllowed(bool v) {
    threatAwarenessSceneAllowed_ = v;
    if (followCamera_)
      followCamera_->SetIdleRecenterSuppressed(v);
  }
  bool IsThreatAwarenessAllowed() const { return threatAwarenessSceneAllowed_; }
  void SetThreatTargetPositions(const std::vector<Vector3> &positions) {
    threatTargetPositions_ = positions;
  }
  void ClearThreatTargetPositions() { threatTargetPositions_.clear(); }
  void FaceDefeatNextEnemy(const Vector3 &enemyWorldPos);

  // ============================================================
  // 見切れヒット演出
  // 画面外（フレーム外）の敵に攻撃がヒットした瞬間に呼ぶ。敵が画角外なら
  // 「決めカメラ」（背後リセンター＋ズーム寄り＋シェイク）を1回だけ発火し、
  // 見失った敵を捉え直す。ロックオンと違い対象を固定せず素早く戻る。
  // 武器クラスのヒット検出から enemyWorldPos を渡して呼ぶ。
  // ============================================================
  void OnAttackHit(const Vector3 &enemyWorldPos);

  // 見切れヒット演出が発火した時 true を1回返し、対象敵のワールド座標を out
  // に渡す （ロックオンUIフラッシュのトリガ＋敵追従用）。GameScene
  // が毎フレーム拾う。
  bool ConsumeLockOnFlashRequest(Vector3 &outWorldPos) {
    if (!lockOnFlashRequested_)
      return false;
    lockOnFlashRequested_ = false;
    outWorldPos = lockOnFlashWorldPos_;
    return true;
  }

  // カメラ追従（右スティックでプレイヤーも回る）フラグのポインタを受け取る。
  // PlayerMovement の config フラグを指し、カメラエディタから ON/OFF
  // できるようにする。 extension JSON
  // に保存済みの値があれば復元する（Initialize() 直後に呼ばれる想定）。
  void SetCameraFollowFlag(bool *p) {
    cameraFollowEnabled_ = p;
    if (!cameraFollowEnabled_ || !followCamera_)
      return;

    const auto &ext = followCamera_->GetExtensionJson();
    if (ext.contains("cameraFollowEnabled")) {
      *cameraFollowEnabled_ = ext["cameraFollowEnabled"];
    }
  }

  // ============================================================
  // FollowCamera への委譲
  // ============================================================
  FollowCamera *GetFollowCamera() const { return followCamera_; }

  void SetIsCloseUp(bool v);
  void StartShake(float intensity, float dur) {
    if (followCamera_)
      followCamera_->StartShake(intensity, dur);
  }
  void StartZoom(float fov, float dur) {
    if (followCamera_)
      followCamera_->StartZoom(fov, dur);
  }
  Vector3 GetRotate() const {
    return followCamera_ ? followCamera_->GetRotate() : Vector3{};
  }
  void SetRotate(const Vector3 &r) {
    if (followCamera_)
      followCamera_->SetRotate(r);
  }

  // ============================================================
  // コンポーネントアクセッサ
  // ============================================================
  AttackCameraComponent &GetAttackCameraComponent() { return attackCamera_; }

  std::vector<std::string> GetAttackCameraNames() const {
    return attackCamera_.GetWorkNames();
  }

  // ============================================================
  // 最終カメラのアクセッサ
  // ============================================================
  YoRigine::Camera *GetLastCamera() { return lastSceneCamera_; }

private:
  // ============================================================
  // 内部処理
  // ============================================================
  void UpdateStickInput();
  void UpdateLockOn();
  void SwitchLockOnTarget(int direction); // 0=最近傍, 1=右, -1=左

  // カメラ遮蔽フェード：フェードモードのコライダーが検出された場合に対象
  // Object3d を半透明化する
  void UpdateOcclusionFade(float dt);

  // ============================================================
  // 脅威察知（気配）
  // ロックオンのように特定の敵を中央へ固定するのではなく、周囲の敵の
  // 「気配」をそれとなく伝える。フリーカメラ時のみ動作する。
  // 周辺視グランス：視界外/背後に敵がいたら、その方向へごく軽く
  //      （awarenessMaxYaw_ まで）カメラを傾ける。完全には振り向かない。
  // 囲まれFOV：近くの敵が増えるほどFOVを少し広げて状況を見渡せるようにする。
  // ============================================================
  void UpdateThreatAwareness(float dt); // pre-director：周辺視グランス
  void ApplyThreatFovWiden(YoRigine::Camera *sceneCamera,
                           float dt); // post-director：囲まれFOV拡大
  int GatherNearbyEnemies(std::vector<BaseCollider *> &out) const;
  int GatherThreatTargetPositions(std::vector<Vector3> &out) const;
  float ComputeGlanceBias() const;

  // 脅威察知パラメータの永続化（FollowCamera の extension JSON に相乗りさせる）
  void SaveThreatAwareness(nlohmann::json &j) const;
  void LoadThreatAwareness(const nlohmann::json &j);

  // ロックオン2ショット・フレーミングの永続化（同じく extension JSON に相乗り）
  void SaveLockOnFraming(nlohmann::json &j) const;
  void LoadLockOnFraming(const nlohmann::json &j);

  // ============================================================
  // 見切れヒット演出（内部）
  //   IsWorldPosOffscreen … 最終カメラの view-projection で NDC
  //   投影し画角外か判定 TriggerOffscreenHitReaction …
  //   背後リセンター＋ズーム＋シェイクを発火
  // ============================================================
  bool IsWorldPosOffscreen(const Vector3 &worldPos) const;
  // enemyWorldPos
  // を渡すと「プレイヤー→敵の方向」へ背後リセンター（敵を確実に正面へ）。
  // nullptr
  // ならプレイヤーの向きへ背後リセンター（テスト発火等のフォールバック）。
  void TriggerOffscreenHitReaction(const Vector3 *enemyWorldPos = nullptr);
  void SaveOffscreenHitReaction(nlohmann::json &j) const;
  void LoadOffscreenHitReaction(const nlohmann::json &j);

  // ============================================================
  // 最終フレーミングガード
  // 攻撃カメラワークのオフセット適用後に呼び、プレイヤーが
  // 画角のハードリミットを越えていたら yaw / pitch を引き戻して
  // 確実にフレーム内へ収める（オフセットを見落とす EnsureTargetInView
  // の後段ガード）。
  // ============================================================
  void EnsurePlayerInFrame(YoRigine::Camera *sceneCamera, float dt);

  // ============================================================
  // 攻撃カメラワーク：参照フレーム / 注視
  //   BuildOffsetFrame … posOffset を解釈する基底（回転行列）を組む
  //   ResolveLookAtPos … 注視対象のワールド座標を返す（対象なしなら false）
  //   PlayerPivotWorld … プレイヤーピボット（pivotHeight 込み）のワールド座標
  // ============================================================
  Matrix4x4 BuildOffsetFrame(CameraSpace space,
                             const YoRigine::Camera *sceneCamera) const;
  bool ResolveLookAtPos(LookAtTarget target, Vector3 &outPos) const;
  Vector3 PlayerPivotWorld() const;
  void ApplyLookAt(YoRigine::Camera *sceneCamera, LookAtTarget target,
                   float weight) const;

  // ============================================================
  // メンバ
  // ============================================================

  // ---- コア参照 ----
  FollowCamera *followCamera_ = nullptr;
  const YoRigine::WorldTransform *playerWT_ = nullptr;

  // PlayerMovement
  // の「カメラ追従でプレイヤーも回る」フラグ（カメラエディタから切替）
  bool *cameraFollowEnabled_ = nullptr;

  // ---- 攻撃カメラワーク ----
  AttackCameraComponent attackCamera_;

  // 注視/参照基準アンカー（Play 時に渡された任意座標）
  Vector3 cameraWorkAnchor_ = {};
  bool hasCameraWorkAnchor_ = false;

  // ---- 戦闘開始演出（Debug / Release どちらでも動作する） ----
  std::shared_ptr<BattleStartCameraState> battleStartState_;
  bool isPreviewMode_ = false;

#ifdef USE_IMGUI
  AttackCameraEditor editor_; // 攻撃カメラワークエディタ
#endif

  // ---- ロックオン ----
  BaseCollider *lockedTarget_ = nullptr;
  bool isLockOn_ = false;
  float lockOnSwitchCooldown_ = 0.0f;

  // ---- ロックオン2ショット・フレーミング ----
  // カメラを player→enemy 軸の真後ろに置くとプレイヤーが敵を隠す（被り）。
  // ヨーに肩オフセットを足して軸からずらし、見下ろしで2体を縦に分離する。
  float lockOnShoulderYaw_ =
      0.25f; // 肩オフセット(rad)。0=真後ろ(被る)、±で左右の肩(≒14°)
  float lockOnPitchBias_ = 0.20f; // 見下ろし加算(rad)。2体を縦に分離(≒11°)
  float lockOnLerpSpeed_ = 10.0f; // ロックオン追従の補間速度

  // スティック入力で使う pitch 制限（FollowCamera から同期）
  float minPitch_ = -0.2f;
  float maxPitch_ = 1.2f;
  float rotateSpeed_ = 0.1f;
  float pivotHeight_ = 1.5f;

  // ============================================================
  // カメラ操作フィール（右スティック）。dt 基準・アナログ・カーブ・加速。
  //   extension JSON("cameraFeel") に永続化し、追従カメラ設定パネルで調整可能。
  // ============================================================
  float camYawSpeed_ = 3.2f;   // ヨー最大角速度(rad/s)
  float camPitchSpeed_ = 2.4f; // ピッチ最大角速度(rad/s)

  // 戦闘中の RB/LB 押しっぱなし回転（yaw）の角速度(rad/s)。JSON:
  // "bumperYawSpeed"
  bool battleMode_ = false;
  float bumperRotateSpeed_ = 2.6f;
  float camDeadzone_ = 0.18f; // ラジアルデッドゾーン(0..1)
  float camResponseCurve_ =
      2.0f; // レスポンスカーブ指数(1=線形 / 大きいほど中央が精密)
  float camAccelTime_ = 0.12f; // 0→最大入力までの加速時間(秒)。0で即時
  bool camInvertY_ = false;    // ピッチ反転
  float camRampState_ = 0.0f;  // 加速ランプの内部状態(0..1)

  void SaveCameraFeel(nlohmann::json &j) const;
  void LoadCameraFeel(const nlohmann::json &j);

  // ============================================================
  // 最終フレーミングガード用パラメータ
  // ============================================================
  bool framingGuardEnabled_ = true; // ガード自体の ON / OFF
  float framingHardLimitX_ =
      0.85f; // 横方向ハードリミット（NDC -1〜1、これを越えたら引き戻す）
  float framingHardLimitY_ = 0.80f; // 縦方向ハードリミット（NDC -1〜1）
  float framingGuardSpeed_ =
      12.0f; // 引き戻し速度（rad/秒・はみ出し分に対する最大変化量）

  // ============================================================
  // 脅威察知（気配）用パラメータ
  // ============================================================
  bool threatAwarenessEnabled_ = true; // 機能マスタースイッチ（デザイナ調整用）
  bool threatAwarenessSceneAllowed_ =
      false; // 現在のシーンで許可されているか（バトル中のみ true）
  std::vector<Vector3>
      threatTargetPositions_;    // BattleScene から渡される生存中の敵位置
  float awarenessRange_ = 25.0f; // この距離内の敵を「気配」対象にする

  // 周辺視グランス
  float awarenessTriggerYaw_ =
      0.70f; // カメラ前方からこの角(rad)以上外れた敵を対象に(≒40°)
  float awarenessMaxYaw_ =
      0.18f; // グランスの最大ヨー量(rad)(≒10°)。これ以上は向かない＝固定しない
  float awarenessYawSpeed_ = 4.0f; // グランスの追従速度
  float awarenessYawBias_ = 0.0f;  // 現在のグランス量（内部状態）
  float awarenessAppliedBias_ =
      0.0f; // 前フレームに yaw へ加算した量（テレスコープ適用用）

  // 囲まれFOV
  int awarenessFovMinCount_ = 2;       // この体数以上で広げ始める
  float awarenessFovPerEnemy_ = 0.03f; // 敵1体ごとに広げるFOV(rad)
  float awarenessFovMax_ = 0.12f;      // FOV拡大の上限(rad)
  float awarenessFovSpeed_ = 3.0f;     // FOV補間速度
  float awarenessFovBias_ = 0.0f;      // 現在のFOV拡大量（内部状態）

  bool stickActiveThisFrame_ = false; // 今フレーム右スティック操作があったか

  // ============================================================
  // 見切れヒット演出 用パラメータ（extension JSON に相乗り）
  // ============================================================
  YoRigine::Camera *lastSceneCamera_ =
      nullptr; // ApplyPostDirector でキャッシュした最終カメラ（見切れ判定用）

  bool offscreenHitEnabled_ = true; // 機能マスタースイッチ
  float offscreenHitMargin_ =
      0.9f; // NDC これを越えたら画面外扱い（0.9=端で切れかけ
            // / 1.0=完全に枠外のみ）
  float offscreenHitCooldown_ = 0.8f; // 連続発火防止の間隔（秒）
  float offscreenHitZoomFov_ =
      0.40f; // 寄りの目標FOV（基準より小さいほど寄る・0でズームなし）
  float offscreenHitZoomDur_ = 0.30f;  // ズーム時間（秒）
  float offscreenHitShakeInt_ = 0.30f; // シェイク強度
  float offscreenHitShakeDur_ = 0.15f; // シェイク時間（秒）
  float offscreenHitProtect_ =
      0.12f; // 決めカメラの敵向きリセンターを保護する時間（秒）。この間はスティック等で中断されない（0で無効）
  float offscreenHitCdTimer_ = 0.0f; // クールダウン残り（内部状態）
  bool lockOnFlashRequested_ =
      false; // 見切れ演出発火→UIフラッシュ要求（GameSceneが消費）
  Vector3 lockOnFlashWorldPos_ = {}; // フラッシュを出す対象（敵）のワールド座標

  // ============================================================
  // カメラ遮蔽フェード
  // ============================================================
  // 遮蔽中と復帰中の全オブジェクトを個別に追跡する。
  std::unordered_map<ObjectManager::PlacedObject *, float> fadeOccluders_;
  float occludeFadeInSpeed_ = 10.0f; // 遮蔽時の追従速度（/秒）
  float occludeFadeOutSpeed_ = 7.0f; // 遮蔽解除時の復帰速度（/秒）
  // 各目標アルファは CameraCollisionResolver::GetFadeHits()
  // から毎フレーム取得する (侵入深度に応じて 0.25〜0.0 の間を動く)
};
