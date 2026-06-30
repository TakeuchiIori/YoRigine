#pragma once
#include "../VirtualCamera.h"
#include <WorldTransform/WorldTransform.h>
#include "CameraState.h"
#include "../../CameraCollisionResolver.h"

/// <summary>
/// ターゲット追従カメラ。
/// 責務：位置計算（FollowProcess）・シェイク・ズーム・コリジョン回避。
/// ゲーム固有のロックオン / BattleStart 演出は PlayerCamera (YGame) が担う。
/// </summary>
class FollowCamera : public VirtualCamera
{
public:
    // ============================================================
    // 基本関数
    // ============================================================
    void Initialize() override;
    void Update() override;
    void DrawDebugGui() override;

    void Save(nlohmann::json& j) const override;
    void Load(const nlohmann::json& j) override;

    // ============================================================
    // ターゲット管理
    // ============================================================
    void SetTarget(const WorldTransform* target, const std::string& name) {
        target_ = target;
        targetName_ = name;
    }
    const std::string& GetTargetName() const { return targetName_; }
    const WorldTransform* GetTarget() const { return target_; }

    void SetIsCloseUp(bool v) { isCloseUp_ = v; }
    bool GetIsCloseUp()   const { return isCloseUp_; }

    CameraState* GetCurrentState() const { return currentState_.get(); }

    // ============================================================
    // カメラ制御
    // ============================================================
    void ChangeState(std::unique_ptr<CameraState> newState);
    void GetDefaultCameraParams(Vector3& outPos, Vector3& outRot, float& outFov) const;
    bool IsInPerformance() const { return currentState_ && currentState_->IsPerformance(); }

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
    bool GetInputEnabled()  const { return inputEnabled_; }

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
    void RecenterToYaw(float targetWorldYaw, bool resetPitch = true);
    bool IsRecentering() const { return recentering_; }
    void CancelRecenter()      { recentering_ = false; }

    // ============================================================
    // フレーミング補正（追従対象が画角外に出そうな時だけ、
    // はみ出した分を rotation で穏やかに pivot へ引き戻す）
    // ============================================================
    void SetFramingEnabled(bool e)  { framingEnabled_ = e; }
    bool IsFramingEnabled() const   { return framingEnabled_; }

    // ============================================================
    // パラメータ読み取り (PlayerCamera が再利用するために公開)
    // ============================================================
    float GetBaseFovY()          const { return baseFovY_; }
    float GetMinPitch()          const { return minPitch_; }
    float GetMaxPitch()          const { return maxPitch_; }
    float GetRotateSpeed()       const { return kRotateSpeed_; }
    float GetTargetPivotHeight() const { return targetPivot_Height_; }

private:
    // ============================================================
    // ターゲット
    // ============================================================
    const WorldTransform* target_ = nullptr;
    std::string targetName_;

    // ============================================================
    // ステート
    // ============================================================
    std::unique_ptr<CameraState> currentState_;

    // ============================================================
    // 追従パラメータ
    // ============================================================
    Vector3 offset_      = { 0.0f, 6.0f, -40.0f };
    float kRotateSpeed_  = 0.1f;

    // ============================================================
    // 追従スムージング（位置ダンピング）
    //   理想位置へ瞬間スナップせず、臨界減衰スプリングで滑らかに寄せる。
    //   smoothTime が小さいほどキビキビ、大きいほどフワッと遅れて追従する。
    // ============================================================
    bool    positionSmoothing_  = true;
    float   positionSmoothTime_ = 0.12f;   // 秒。追従の遅れ時間
    float   maxFollowSpeed_     = 300.0f;  // 追従速度上限（暴れ防止）
    float   followSnapDistance_ = 30.0f;   // この距離以上ズレたら瞬間スナップ（テレポート対策）
    Vector3 followPos_          = {};      // 平滑化後のカメラ位置（内部状態）
    Vector3 followVel_          = {};      // SmoothDamp 用の速度アキュムレータ
    bool    followInitialized_  = false;

    // ============================================================
    // 速度先読み（look-ahead）
    //   ターゲットの移動速度に応じて注視点を進行方向へ先行させ、
    //   走行中に前方が見える「予測カメラ」にする。
    // ============================================================
    bool    lookAheadEnabled_  = true;
    bool    lookAheadVertical_ = false;   // Y方向（落下/ジャンプ）も先読みするか
    float   lookAheadTime_     = 0.25f;   // 速度×この秒数だけ先を見る
    float   lookAheadMaxDist_  = 6.0f;    // 先読み距離の上限
    float   lookAheadSmooth_   = 6.0f;    // 先読みのイーズ速度（大きいほど即応）
    Vector3 smoothedLookAhead_ = {};      // 平滑化後の先読みオフセット（内部状態）
    Vector3 prevTargetPos_     = {};      // 速度算出用の前フレーム位置
    bool    hasPrevTargetPos_  = false;

    // ============================================================
    // クローズアップ
    // ============================================================
    bool  isCloseUp_     = false;
    float closeUpScale_  = 0.3f;
    float interpSpeed_   = 5.0f;
    float currentScale_  = 1.0f;

    // ============================================================
    // シェイク
    // ============================================================
    void UpdateShake();
    Vector3 shakeOffset_    = {};
    float   shakeIntensity_ = 0.0f;
    float   shakeDuration_  = 0.0f;
    float   shakeTimer_     = 0.0f;

    // ============================================================
    // ズーム
    // ============================================================
    float baseFovY_      = 0.45f;
    float targetZoomFov_ = 0.45f;
    float zoomDuration_  = 0.0f;
    float zoomTimer_     = 0.0f;

    // ============================================================
    // 縦回転制限 (ラジアン)
    // ============================================================
    float minPitch_ = -0.2f;
    float maxPitch_ =  1.2f;

    // ============================================================
    // 入力制御フラグ
    // ============================================================
    bool inputEnabled_ = true;

    // ============================================================
    // リセンター（対象 facing 背後へ素早く回す）
    // ============================================================
    void  UpdateRecenter(float dt);
    bool  recentering_        = false;
    float recenterTimer_      = 0.0f;
    float recenterDuration_   = 0.18f;  // 寄せにかける時間（秒）。0 で即時
    float recenterFromYaw_    = 0.0f;
    float recenterToYaw_      = 0.0f;
    float recenterFromPitch_  = 0.0f;
    float recenterToPitch_    = 0.0f;
    bool  recenterResetPitch_ = true;   // pitch も既定値へ戻すか
    float recenterPitch_      = 0.30f;  // pitch リセット先（rad）

    // ============================================================
    // Game 拡張用の不透明 JSON ストレージ
    // PlayerCamera が BattleStartState 等のデータを Here に保存/復元する。
    // FollowCamera 自身はこの内容を解釈しない。
    // ============================================================
    nlohmann::json extensionJson_;

public:
    void SetExtensionJson(const nlohmann::json& j) { extensionJson_ = j; }
    const nlohmann::json& GetExtensionJson() const  { return extensionJson_; }
private:

    // ============================================================
    // コリジョン回避コンポーネント
    // ============================================================
    CameraCollisionResolver collisionResolver_;
    float targetPivot_Height_ = 1.5f;

    // ============================================================
    // フレーミング補正（画角外に出そうな時だけ pivot を引き戻す）
    // ============================================================
    void EnsureTargetInView(const Vector3& pivot, float dt);

    bool  framingEnabled_      = true;
    float framingYawMargin_    = 0.45f;  // この角度差(rad)を越えたら yaw 補正開始（≒26°）
    float framingPitchMargin_  = 0.30f;  // 同 pitch 補正開始（≒17°）
    float framingSpeed_        = 4.0f;   // 補正速度（rad/秒・はみ出し分に対する最大変化量）
};
