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
#include <vector>

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

    /// Anchor 注視（LookAtTarget::Anchor）や TargetRelative の基準に使う
    /// 任意のワールド座標（攻撃ヒット点など）を渡して再生する。
    void PlayAttackCameraWork(const std::string& attackName, const Vector3& anchor);

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
    // 脅威察知のシーン許可
    // フィールドでは背景・マップ見渡しを優先して OFF、バトル中だけ ON にする。
    // 各サブシーンの OnEnter から呼ぶ。
    // ============================================================
    void SetThreatAwarenessAllowed(bool v) { threatAwarenessSceneAllowed_ = v; }
    bool IsThreatAwarenessAllowed() const   { return threatAwarenessSceneAllowed_; }

    // ============================================================
    // 見切れヒット演出
    // 画面外（フレーム外）の敵に攻撃がヒットした瞬間に呼ぶ。敵が画角外なら
    // 「決めカメラ」（背後リセンター＋ズーム寄り＋シェイク）を1回だけ発火し、
    // 見失った敵を捉え直す。ロックオンと違い対象を固定せず素早く戻る。
    // 武器クラスのヒット検出から enemyWorldPos を渡して呼ぶ。
    // ============================================================
    void OnAttackHit(const Vector3& enemyWorldPos);

    // 見切れヒット演出が発火した時 true を1回返し、対象敵のワールド座標を out に渡す
    // （ロックオンUIフラッシュのトリガ＋敵追従用）。GameScene が毎フレーム拾う。
    bool ConsumeLockOnFlashRequest(Vector3& outWorldPos) {
        if (!lockOnFlashRequested_) return false;
        lockOnFlashRequested_ = false;
        outWorldPos = lockOnFlashWorldPos_;
        return true;
    }

    // カメラ追従（右スティックでプレイヤーも回る）フラグのポインタを受け取る。
    // PlayerMovement の config フラグを指し、カメラエディタから ON/OFF できるようにする。
    void SetCameraFollowFlag(bool* p) { cameraFollowEnabled_ = p; }

    // ============================================================
    // FollowCamera への委譲
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

    std::vector<std::string> GetAttackCameraNames() const {
        return attackCamera_.GetWorkNames();
    }

private:
    // ============================================================
    // 内部処理
    // ============================================================
    void UpdateStickInput();
    void UpdateLockOn();
    void SwitchLockOnTarget(int direction);  // 0=最近傍, 1=右, -1=左

    // ============================================================
    // 脅威察知（気配）
    // ロックオンのように特定の敵を中央へ固定するのではなく、周囲の敵の
    // 「気配」をそれとなく伝える。フリーカメラ時のみ動作する。
    //   ① 周辺視グランス：視界外/背後に敵がいたら、その方向へごく軽く
    //      （awarenessMaxYaw_ まで）カメラを傾ける。完全には振り向かない。
    //   ② 囲まれFOV：近くの敵が増えるほどFOVを少し広げて状況を見渡せるようにする。
    // ============================================================
    void  UpdateThreatAwareness(float dt);                    // pre-director：周辺視グランス
    void  ApplyThreatFovWiden(Camera* sceneCamera, float dt); // post-director：囲まれFOV拡大
    int   GatherNearbyEnemies(std::vector<BaseCollider*>& out) const;
    float ComputeGlanceBias() const;

    // 脅威察知パラメータの永続化（FollowCamera の extension JSON に相乗りさせる）
    void  SaveThreatAwareness(nlohmann::json& j) const;
    void  LoadThreatAwareness(const nlohmann::json& j);

    // ロックオン2ショット・フレーミングの永続化（同じく extension JSON に相乗り）
    void  SaveLockOnFraming(nlohmann::json& j) const;
    void  LoadLockOnFraming(const nlohmann::json& j);

    // ============================================================
    // 見切れヒット演出（内部）
    //   IsWorldPosOffscreen … 最終カメラの view-projection で NDC 投影し画角外か判定
    //   TriggerOffscreenHitReaction … 背後リセンター＋ズーム＋シェイクを発火
    // ============================================================
    bool  IsWorldPosOffscreen(const Vector3& worldPos) const;
    // enemyWorldPos を渡すと「プレイヤー→敵の方向」へ背後リセンター（敵を確実に正面へ）。
    // nullptr ならプレイヤーの向きへ背後リセンター（テスト発火等のフォールバック）。
    void  TriggerOffscreenHitReaction(const Vector3* enemyWorldPos = nullptr);
    void  SaveOffscreenHitReaction(nlohmann::json& j) const;
    void  LoadOffscreenHitReaction(const nlohmann::json& j);

    // ============================================================
    // 最終フレーミングガード
    // 攻撃カメラワークのオフセット適用後に呼び、プレイヤーが
    // 画角のハードリミットを越えていたら yaw / pitch を引き戻して
    // 確実にフレーム内へ収める（オフセットを見落とす EnsureTargetInView の後段ガード）。
    // ============================================================
    void EnsurePlayerInFrame(Camera* sceneCamera, float dt);

    // ============================================================
    // 攻撃カメラワーク：参照フレーム / 注視
    //   BuildOffsetFrame … posOffset を解釈する基底（回転行列）を組む
    //   ResolveLookAtPos … 注視対象のワールド座標を返す（対象なしなら false）
    //   PlayerPivotWorld … プレイヤーピボット（pivotHeight 込み）のワールド座標
    // ============================================================
    Matrix4x4 BuildOffsetFrame(CameraSpace space, const Camera* sceneCamera) const;
    bool      ResolveLookAtPos(LookAtTarget target, Vector3& outPos) const;
    Vector3   PlayerPivotWorld() const;
    void      ApplyLookAt(Camera* sceneCamera, LookAtTarget target, float weight) const;

    // ============================================================
    // メンバ
    // ============================================================
    FollowCamera*          followCamera_ = nullptr;
    const WorldTransform*  playerWT_     = nullptr;

    // PlayerMovement の「カメラ追従でプレイヤーも回る」フラグ（カメラエディタから切替）
    bool* cameraFollowEnabled_ = nullptr;

    AttackCameraComponent  attackCamera_;

    // 攻撃カメラワークの注視/参照基準アンカー（Play 時に渡された任意座標）
    Vector3 cameraWorkAnchor_   = {};
    bool    hasCameraWorkAnchor_ = false;

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

    // ---- ロックオン2ショット・フレーミング ----
    // カメラを player→enemy 軸の真後ろに置くとプレイヤーが敵を隠す（被り）。
    // ヨーに肩オフセットを足して軸からずらし、見下ろしで2体を縦に分離する。
    float lockOnShoulderYaw_ = 0.25f;  // 肩オフセット(rad)。0=真後ろ(被る)、±で左右の肩(≒14°)
    float lockOnPitchBias_   = 0.20f;  // 見下ろし加算(rad)。2体を縦に分離(≒11°)
    float lockOnLerpSpeed_   = 10.0f;  // ロックオン追従の補間速度

    // スティック入力で使う pitch 制限（FollowCamera から同期）
    float minPitch_     = -0.2f;
    float maxPitch_     =  1.2f;
    float rotateSpeed_  = 0.1f;
    float pivotHeight_  = 1.5f;

    // ============================================================
    // 最終フレーミングガード用パラメータ
    // ============================================================
    bool  framingGuardEnabled_ = true;   // ガード自体の ON / OFF
    float framingHardLimitX_   = 0.85f;  // 横方向ハードリミット（NDC -1〜1、これを越えたら引き戻す）
    float framingHardLimitY_   = 0.80f;  // 縦方向ハードリミット（NDC -1〜1）
    float framingGuardSpeed_   = 12.0f;  // 引き戻し速度（rad/秒・はみ出し分に対する最大変化量）

    // ============================================================
    // 脅威察知（気配）用パラメータ
    // ============================================================
    bool  threatAwarenessEnabled_      = true;   // 機能マスタースイッチ（デザイナ調整用）
    bool  threatAwarenessSceneAllowed_ = false;  // 現在のシーンで許可されているか（バトル中のみ true）
    float awarenessRange_      = 25.0f;  // この距離内の敵を「気配」対象にする

    // ① 周辺視グランス
    float awarenessTriggerYaw_ = 0.70f;  // カメラ前方からこの角(rad)以上外れた敵を対象に(≒40°)
    float awarenessMaxYaw_     = 0.18f;  // グランスの最大ヨー量(rad)(≒10°)。これ以上は向かない＝固定しない
    float awarenessYawSpeed_   = 4.0f;   // グランスの追従速度
    float awarenessYawBias_     = 0.0f;  // 現在のグランス量（内部状態）
    float awarenessAppliedBias_ = 0.0f;  // 前フレームに yaw へ加算した量（テレスコープ適用用）

    // ② 囲まれFOV
    int   awarenessFovMinCount_ = 2;      // この体数以上で広げ始める
    float awarenessFovPerEnemy_ = 0.03f;  // 敵1体ごとに広げるFOV(rad)
    float awarenessFovMax_      = 0.12f;  // FOV拡大の上限(rad)
    float awarenessFovSpeed_    = 3.0f;   // FOV補間速度
    float awarenessFovBias_     = 0.0f;   // 現在のFOV拡大量（内部状態）

    bool  stickActiveThisFrame_ = false;  // 今フレーム右スティック操作があったか

    // ============================================================
    // 見切れヒット演出 用パラメータ（extension JSON に相乗り）
    // ============================================================
    Camera* lastSceneCamera_ = nullptr;     // ApplyPostDirector でキャッシュした最終カメラ（見切れ判定用）

    bool  offscreenHitEnabled_   = true;    // 機能マスタースイッチ
    bool  offscreenHitFaceEnemy_ = true;    // true=プレイヤー→敵方向へ背後リセンター(確実) / false=プレイヤーの向きへ
    float offscreenHitMargin_   = 0.9f;     // NDC これを越えたら画面外扱い（0.9=端で切れかけ / 1.0=完全に枠外のみ）
    float offscreenHitCooldown_ = 0.8f;     // 連続発火防止の間隔（秒）
    float offscreenHitZoomFov_  = 0.40f;    // 寄りの目標FOV（基準より小さいほど寄る・0でズームなし）
    float offscreenHitZoomDur_  = 0.30f;    // ズーム時間（秒）
    float offscreenHitShakeInt_ = 0.30f;    // シェイク強度
    float offscreenHitShakeDur_ = 0.15f;    // シェイク時間（秒）
    float offscreenHitCdTimer_  = 0.0f;     // クールダウン残り（内部状態）
    bool    lockOnFlashRequested_ = false;  // 見切れ演出発火→UIフラッシュ要求（GameSceneが消費）
    Vector3 lockOnFlashWorldPos_  = {};     // フラッシュを出す対象（敵）のワールド座標
};
