#pragma once
#include "AttackCameraComponent.h"

#ifdef USE_IMGUI
#include "AttackCameraEditor.h"
#endif

#include "Systems/Camera/Virtuals/FollowCamera/FollowCamera.h"
#include "Systems/Camera/Virtuals/FollowCamera/BattleStartCameraState.h"
#include "WorldTransform/WorldTransform.h"
#include "Collision/Core/BaseCollider.h"

#include <memory>
#include <string>

/// <summary>
/// プレイヤー専用カメラ管理クラス（Game 層のアグリゲーター）。
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
    void Initialize(FollowCamera* followCamera, const WorldTransform* playerWT);

    // スティック入力・ロックオン更新（CameraDirector::Update の前に呼ぶ）
    void UpdatePreDirector();

    // 攻撃カメラオフセットを sceneCamera に適用（CameraDirector::Update の後に呼ぶ）
    void ApplyPostDirector(Camera* sceneCamera, float dt);

    void DrawImGui();

    // ============================================================
    // 攻撃カメラワーク
    // ============================================================
    void PlayAttackCameraWork(const std::string& attackName);
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
    BaseCollider* GetLockedTarget() const { return lockedTarget_; }
    bool          IsLockOn()        const { return isLockOn_; }

    // ============================================================
    // FollowCamera への委譲 API（後方互換）
    // ============================================================
    FollowCamera* GetFollowCamera() const { return followCamera_; }

    void    SetIsCloseUp(bool v)                  { if (followCamera_) followCamera_->SetIsCloseUp(v); }
    void    StartShake(float intensity, float dur) { if (followCamera_) followCamera_->StartShake(intensity, dur); }
    void    StartZoom(float fov, float dur)        { if (followCamera_) followCamera_->StartZoom(fov, dur); }
    Vector3 GetRotate()   const { return followCamera_ ? followCamera_->GetRotate()    : Vector3{}; }
    void    SetRotate(const Vector3& r)            { if (followCamera_) followCamera_->SetRotate(r); }

    // ============================================================
    // コンポーネントアクセッサ
    // ============================================================
    AttackCameraComponent& GetAttackCameraComponent() { return attackCamera_; }

private:
    // ============================================================
    // 内部処理
    // ============================================================
    void UpdateStickInput();
    void UpdateLockOn();
    void SwitchLockOnTarget(int direction);  // 0=最近傍, 1=右, -1=左

    // ============================================================
    // メンバ
    // ============================================================
    FollowCamera*          followCamera_ = nullptr;
    const WorldTransform*  playerWT_     = nullptr;

    AttackCameraComponent  attackCamera_;

    // BattleStart 演出：Debug / Release どちらでも動作する
    std::shared_ptr<BattleStartCameraState> battleStartState_;
    bool isPreviewMode_ = false;

#ifdef USE_IMGUI
    AttackCameraEditor editor_;
#endif

    // ---- ロックオン ----
    BaseCollider* lockedTarget_          = nullptr;
    bool          isLockOn_              = false;
    float         lockOnSwitchCooldown_  = 0.0f;

    // スティック入力で使う pitch 制限（FollowCamera から同期）
    float minPitch_     = -0.2f;
    float maxPitch_     =  1.2f;
    float rotateSpeed_  = 0.1f;
    float pivotHeight_  = 1.5f;
};
