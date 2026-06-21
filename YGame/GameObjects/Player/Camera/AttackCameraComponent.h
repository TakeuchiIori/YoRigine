#pragma once
#include "AttackCameraWork.h"
#include <string>
#include <vector>

class FollowCamera;

/// <summary>
/// 攻撃カメラワーク再生コンポーネント。
///
/// 再生フェーズ:
///   StartInterpolating … 開始時の補間中
///   Playing   … キーフレーム補間でオフセットを適用
///   Returning … 演出終了後、returnDuration かけて元の状態へイーズアウト補間
///   Idle      … 非アクティブ（オフセット = 0）
/// </summary>
class AttackCameraComponent {
public:
    // ============================================================
    // 再生フェーズ
    // ============================================================
    enum class Phase { Idle, StartInterpolating, Playing, Returning };

    // ============================================================
    // 基本関数
    // ============================================================
    void Initialize();

    /// フェーズ1: タイマー前進 + シェイクトリガー（CameraDirector の前に呼ぶ）
    void UpdatePre(FollowCamera* followCamera, float dt);

    /// フェーズ2: 補間値をサンプリングして現在値を更新（CameraDirector の後に呼ぶ）
    void UpdatePost(float dt);

    // ============================================================
    // 再生制御
    // ============================================================
    void Play(const std::string& workName);
    void Stop(FollowCamera* camera);   // 即座に停止（フェーズをスキップ）

    /// Playing 中 true（StartInterpolating は含まない）
    bool IsPlaying()   const { return phase_ == Phase::Playing; }
    bool IsReturning() const { return phase_ == Phase::Returning; }
    bool IsActive()    const { return phase_ != Phase::Idle; }
    float GetPlayRatio() const;
    const std::string& GetCurrentWorkName() const { return currentWorkName_; }

    /// 現在再生中のワークが「プレイヤーをフレーム内に保持」設定か（該当ワークが無ければ true）
    bool ShouldKeepPlayerInFrame() const;

    // ============================================================
    // データ管理
    // ============================================================
    void AddWork(const AttackCameraWork& work);
    void RemoveWork(const std::string& name);

    AttackCameraWork* FindWork(const std::string& name);
    const AttackCameraWork* FindWork(const std::string& name) const;

    std::vector<AttackCameraWork>& GetWorks() { return works_; }
    const std::vector<AttackCameraWork>& GetWorks() const { return works_; }

    // ============================================================
    // ファイル I/O
    // ============================================================
    void SaveToFile(const std::string& filePath) const;
    void LoadFromFile(const std::string& filePath);

    // ============================================================
    // 現在値アクセッサ（ApplyPostDirector / エディター用）
    // ============================================================
    Vector3 GetCurrentPosOffset() const { return currentPosOffset_; }
    Vector3 GetCurrentRotOffset() const { return currentRotOffset_; }
    float   GetCurrentFovDelta()  const { return currentFovDelta_; }
    float   GetCurrentTimeScale() const { return currentTimeScale_; }
    float   GetCurrentLookAtWeight() const { return currentLookAtWeight_; }

    /// 再生中ワークの参照フレーム / 注視対象（無ければ既定値）
    CameraSpace  GetCurrentPosSpace() const;
    LookAtTarget GetCurrentLookAt()   const;

    void  SetSavedBaseFov(float fov) { savedBaseFov_ = fov; }
    float GetSavedBaseFov()    const { return savedBaseFov_; }

    // ============================================================
    // ワーク名一覧取得
    // ============================================================
    std::vector<std::string> GetWorkNames() const;

private:
    // ============================================================
    // 内部処理
    // ============================================================
    void SampleKeyframes(const AttackCameraWork& work, float t);
    void EnterStartInterpolation(const AttackCameraWork& work);
    void EnterReturning(const AttackCameraWork& work);
    void ResetValues();
    // ============================================================
    // データ
    // ============================================================
    std::vector<AttackCameraWork> works_;

    // ============================================================
    // 再生状態
    // ============================================================
    Phase       phase_ = Phase::Idle;
    float       playTimer_ = 0.0f;
    std::string currentWorkName_;
    std::vector<bool> shakeTriggered_;

    // ---- 開始時補間フェーズ用 ----
    float   startInterpolationTimer_ = 0.0f;
    float   startInterpolationDuration_ = 0.2f;

    // ---- 終了時補間フェーズ用 ----
    float   returnTimer_ = 0.0f;
    float   returnDuration_ = 0.2f;
    Vector3 returnFromPos_ = {};
    Vector3 returnFromRot_ = {};
    float   returnFromFov_ = 0.0f;
    float   returnFromTs_ = 1.0f;
    float   returnFromLookAt_ = 0.0f;

    // ============================================================
    // 補間済みの現在値
    // ============================================================
    Vector3 currentPosOffset_ = {};
    Vector3 currentRotOffset_ = {};
    float   currentFovDelta_ = 0.0f;
    float   currentTimeScale_ = 1.0f;
    float   currentLookAtWeight_ = 0.0f;

    float savedBaseFov_ = 0.45f;
};