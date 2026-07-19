#pragma once
#include "../../CameraCollisionResolver.h"
#include "../VirtualCamera.h"
#include "CameraState.h"
#include <WorldTransform/WorldTransform.h>

/// <summary>
/// ターゲット追従カメラ。
/// 責務：位置計算（FollowProcess）・シェイク・ズーム・コリジョン回避。
/// ゲーム固有のロックオン / BattleStart 演出は PlayerCamera (YGame) が担う。
/// </summary>
class FollowCamera : public VirtualCamera {
public:
  // ============================================================
  // 基本関数
  // ============================================================
  void Initialize() override;
  void Update() override;
  void DrawDebugGui() override;

  void Save(nlohmann::json &j) const override;
  void Load(const nlohmann::json &j) override;

  // ============================================================
  // ターゲット管理
  // ============================================================
  void SetTarget(const WorldTransform *target, const std::string &name) {
    target_ = target;
    targetName_ = name;
  }
  const std::string &GetTargetName() const { return targetName_; }
  const WorldTransform *GetTarget() const { return target_; }

  void SetIsCloseUp(bool v);
  bool GetIsCloseUp() const { return isCloseUp_; }

  CameraState *GetCurrentState() const { return currentState_.get(); }

  // ============================================================
  // カメラ制御
  // ============================================================
  void ChangeState(std::unique_ptr<CameraState> newState);
  void GetDefaultCameraParams(Vector3 &outPos, Vector3 &outRot,
                              float &outFov) const;
  bool IsInPerformance() const {
    return currentState_ && currentState_->IsPerformance();
  }

  // ============================================================
  // シェイク / ズーム
  // ============================================================
  void StartShake(float intensity, float duration);
  void StartZoom(float targetFov, float duration);
  void UpdateZoom();

  // ============================================================
  // 追従・入力 (PlayerCamera から呼ぶ public API)
  // ============================================================
  // false にすると UpdateInput() 内のスティック処理をスキップする
  void SetInputEnabled(bool e) { inputEnabled_ = e; }
  bool GetInputEnabled() const { return inputEnabled_; }

  void UpdateInput();
  void FollowProcess();

  // ============================================================
  // リセンター（追従対象の向いている方向の背後へ素早く回す）
  //   RB / LB などのトリガーで呼ぶ。recenterDuration_ をかけて
  //   yaw を対象 facing へ最短角で寄せ、pitch を既定値へ戻す。
  //   0 秒設定なら即時スナップ。
  // ============================================================
  void RecenterBehindTarget();
  // 指定したワールド yaw（オービット角）へ同じイーズで寄せる。
  //   RecenterBehindTarget が「対象の向き」へ寄せるのに対し、これは任意方向へ。
  //   例: プレイヤー→敵 の方向 yaw を渡せば、カメラがプレイヤー真後ろ
  //   （敵と反対側）へ回り込み、プレイヤー越しに敵を正面に捉える。
  //   duration を省略（負値）すると
  //   recenterDuration_（手動リセンター用の既定値）を使う。
  //   アイドルオートリセンターなど、手動より緩やかに寄せたい場合だけ明示的に渡す。
  //   protectDuration>0 を渡すと、その秒数だけ CancelRecenter(false)
  //   による中断を無視する
  //   （決めカメラ等、開始直後のスティック当たりで消したくない寄せに使う）。
  void RecenterToYaw(float targetWorldYaw, bool resetPitch = true,
                     float duration = -1.0f, float protectDuration = 0.0f);
  bool IsRecentering() const { return recentering_; }
  // force=false:
  // 保護時間中（recenterProtectTimer_>0）は中断しない（スティック等の割り込み用）。
  // force=true : 保護を無視して即中断（別の決め動作で上書きする場合など）。
  void CancelRecenter(bool force = false);

  // ============================================================
  // アイドル時オートリセンター用通知
  //   PlayerCamera 側でスティック操作 / ロックオン照準など「カメラを
  //   能動的に制御している」と判定したフレームで毎回呼ぶ。呼ばれている
  //   間はアイドルタイマーがリセットされ、オートリセンターは発動しない。
  // ============================================================
  void NotifyCameraActive() { idleRecenterTimer_ = 0.0f; }

  // ============================================================
  // アイドル時オートリセンターの一時停止
  //   idleRecenterEnabled_（設計者がプリセットで保存する基本設定）とは別の、
  //   実行時のみの一時停止フラグ。バトル中など「この間だけ止めたい」場合に
  //   Game 側（PlayerCamera）から呼ぶ。プリセットの ON/OFF
  //   設定自体は書き換えない。
  // ============================================================
  void SetIdleRecenterSuppressed(bool s) { idleRecenterSuppressed_ = s; }
  bool IsIdleRecenterSuppressed() const { return idleRecenterSuppressed_; }

  // ============================================================
  // フレーミング補正（追従対象が画角外に出そうな時だけ、
  // はみ出した分を rotation で穏やかに pivot へ引き戻す）
  // ============================================================
  void SetFramingEnabled(bool e) { framingEnabled_ = e; }
  bool IsFramingEnabled() const { return framingEnabled_; }

  // ============================================================
  // パラメータ読み取り (PlayerCamera が再利用するために公開)
  // ============================================================
  float GetBaseFovY() const { return baseFovY_; }
  float GetMinPitch() const { return minPitch_; }
  float GetMaxPitch() const { return maxPitch_; }
  float GetRotateSpeed() const { return kRotateSpeed_; }
  float GetTargetPivotHeight() const { return targetPivot_Height_; }

private:
  // ============================================================
  // ターゲット
  // ============================================================
  const WorldTransform *target_ = nullptr;
  std::string targetName_;

  // ============================================================
  // ステート
  // ============================================================
  std::unique_ptr<CameraState> currentState_;

  // ============================================================
  // 追従パラメータ
  // ============================================================
  Vector3 offset_ = {0.0f, 6.0f, -40.0f};
  float kRotateSpeed_ = 0.1f;

  // ============================================================
  // 追従スムージング（位置ダンピング）
  //   対象の pivot（＝キャラ位置＋先読み）だけに臨界減衰スプリングを掛ける。
  //   スティック回転によるオフセット(offset_)はここでは平滑化せず、
  //   FollowProcess 内でダンピング後の pivot に生の値のまま加算する
  //   （右スティック操作が位置ダンピングの遅延を受けないようにするため）。
  //   smoothTime が小さいほどキビキビ、大きいほどフワッと遅れて追従する。
  // ============================================================
  bool positionSmoothing_ = true;
  float positionSmoothTime_ = 0.12f; // 秒。追従の遅れ時間
  float maxFollowSpeed_ = 300.0f;    // 追従速度上限（暴れ防止）
  float followSnapDistance_ =
      30.0f; // この距離以上ズレたら瞬間スナップ（テレポート対策）
  Vector3 smoothedPivot_ = {}; // 平滑化後の pivot（内部状態）
  Vector3 pivotVel_ = {};      // SmoothDamp 用の速度アキュムレータ
  bool followInitialized_ = false;

  // ============================================================
  // 速度先読み（look-ahead）
  //   ターゲットの移動速度に応じて注視点を進行方向へ先行させ、
  //   走行中に前方が見える「予測カメラ」にする。
  // ============================================================
  bool lookAheadEnabled_ = true;
  bool lookAheadVertical_ = false; // Y方向（落下/ジャンプ）も先読みするか
  float lookAheadTime_ = 0.25f;    // 速度×この秒数だけ先を見る
  float lookAheadMaxDist_ = 6.0f;  // 先読み距離の上限
  float lookAheadSmooth_ = 6.0f;   // 先読みのイーズ速度（大きいほど即応）
  Vector3 smoothedLookAhead_ = {}; // 平滑化後の先読みオフセット（内部状態）
  Vector3 prevTargetPos_ = {};     // 速度算出用の前フレーム位置
  bool hasPrevTargetPos_ = false;

  // ============================================================
  // クローズアップ
  // ============================================================
  bool isCloseUp_ = false;
  float closeUpScale_ = 0.3f;
  float interpSpeed_ = 5.0f;
  float currentScale_ = 1.0f;

  // ============================================================
  // シェイク
  // ============================================================
  void UpdateShake();
  Vector3 shakeOffset_ = {};
  float shakeIntensity_ = 0.0f;
  float shakeDuration_ = 0.0f;
  float shakeTimer_ = 0.0f;

  // ============================================================
  // ズーム
  // ============================================================
  float baseFovY_ = 0.45f;
  float targetZoomFov_ = 0.45f;
  float zoomDuration_ = 0.0f;
  float zoomTimer_ = 0.0f;

  // ============================================================
  // 縦回転制限 (ラジアン)
  // ============================================================
  float minPitch_ = -0.2f;
  float maxPitch_ = 1.2f;

  // ============================================================
  // 入力制御フラグ
  // ============================================================
  bool inputEnabled_ = true;

  // ============================================================
  // リセンター（対象 facing 背後へ素早く回す）
  // ============================================================
  void UpdateRecenter(float dt);
  bool recentering_ = false;
  float recenterTimer_ = 0.0f;
  float recenterProtectTimer_ =
      0.0f; // >0 の間は CancelRecenter(false) を無視（保護時間・内部状態）
  float recenterDuration_ = 0.18f; // 寄せにかける時間（秒）。0 で即時
  float recenterFromYaw_ = 0.0f;
  float recenterToYaw_ = 0.0f;
  float recenterFromPitch_ = 0.0f;
  float recenterToPitch_ = 0.0f;
  bool recenterResetPitch_ = true; // pitch も既定値へ戻すか
  float recenterPitch_ = 0.30f;    // pitch リセット先（rad）
  float activeRecenterDuration_ =
      0.0f; // 今回のリセンターに使うイーズ時間（内部状態。呼び出し側の
            // duration 引数で上書きされる）

  // ============================================================
  // Game 拡張用の不透明 JSON ストレージ
  // PlayerCamera が BattleStartState 等のデータを Here に保存/復元する。
  // FollowCamera 自身はこの内容を解釈しない。
  // ============================================================
  nlohmann::json extensionJson_;

public:
  void SetExtensionJson(const nlohmann::json &j) { extensionJson_ = j; }
  const nlohmann::json &GetExtensionJson() const { return extensionJson_; }

  // CameraCollisionResolver のフェードヒット結果を公開する。
  // ApplyPostDirector (PlayerCamera) から毎フレーム読み取り、該当 Object3d
  // を半透明化する。
  const std::vector<CameraCollisionResolver::FadeHit> &GetFadeHits() const {
    return collisionResolver_.GetFadeHits();
  }

private:
  // ============================================================
  // コリジョン回避コンポーネント
  // ============================================================
  CameraCollisionResolver collisionResolver_;
  float targetPivot_Height_ = 1.5f;

  // ============================================================
  // フレーミング補正（画角外に出そうな時だけ pivot を引き戻す）
  // ============================================================
  void EnsureTargetInView(const Vector3 &pivot, float dt);

  bool framingEnabled_ = true;
  float framingYawMargin_ = 1.05f;   // この角度差(rad)を越えたら yaw
                                     // 補正開始（≒60°・最終セーフティネット）
  float framingPitchMargin_ = 0.55f; // 同 pitch 補正開始（≒31°）
  float framingSpeed_ =
      4.0f; // 補正速度（rad/秒・はみ出し分に対する最大変化量）

  // ============================================================
  // アイドル時オートリセンター
  //   移動方向を追いかけるのではなく、「カメラ操作（スティック / ロックオン
  //   照準など）が一定時間なかった」ときにだけ、対象の向いている方向の
  //   背後へ静かに戻す。プレイヤーが能動的にカメラを構えている間は
  //   NotifyCameraActive() が毎フレーム呼ばれてタイマーがリセットされるため、
  //   一切介入しない＝操作を奪わない。
  // ============================================================
  void UpdateIdleRecenter(float dt);

  bool idleRecenterEnabled_ = true;
  float idleRecenterDelay_ = 2.5f; // 無操作がこの秒数続いたら発動
  float idleRecenterDuration_ =
      1.4f; // 発動時のイーズ時間（RB/LB の手動リセンターより緩やかに）
  float idleRecenterTimer_ = 0.0f; // 内部状態：無操作の継続時間
  bool idleRecenterSuppressed_ =
      false; // 実行時の一時停止（非永続・Game 側から制御）
};
