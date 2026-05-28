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
};
