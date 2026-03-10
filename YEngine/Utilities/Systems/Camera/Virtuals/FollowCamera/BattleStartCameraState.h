#pragma once
#include "CinematicCameraState.h"

/// <summary>
/// 戦闘開始時のカメラワーク
/// </summary>
class BattleStartCameraState : public CinematicCameraState {
public:
    void Enter(FollowCamera* camera) override;
    const char* GetStateName() const override { return "BattleStart"; }

    // 保存・読込
    void Save(nlohmann::json& j) const override;
    void Load(const nlohmann::json& j) override;

    // ImGuiでの編集
    void DrawEditGui() override;

    /// 現在のメンバ変数でターゲット基準に制御点を再構築（プレビュー開始前に呼ぶ）
    void RebuildControlPoints(FollowCamera* camera);

    /// 全パラメータをデフォルト値に戻してから再構築（「リセット」ボタン専用）
    void SetupDefaultControlPoints(FollowCamera* camera);
    bool IsPerformance() const override { return true; }
private:
    void ApplyTargetOffset(FollowCamera* camera);

    // ---- 制御点ごとの編集パラメータ（全てメンバで保持） ----
    static constexpr int kPointCount = 3;

    Vector3 offsets_[kPointCount] = {
        {  0.0f, 15.0f, -20.0f },
        { 10.0f,  5.0f,  -5.0f },
        { -2.0f,  4.0f, -12.0f }
    };
    Vector3 rotations_[kPointCount] = {
        { 0.6f,  0.0f, 0.0f },
        { 0.2f, -0.8f, 0.0f },
        { 0.1f,  0.0f, 0.0f }
    };
    float fovs_[kPointCount] = { 0.5f,  0.45f, 0.45f };
    float controlPointTimes_[kPointCount] = { 0.6f,  0.5f,  0.6f };

    float returnInterpTime_ = 0.5f;
    bool  lookAtTargetFlag_ = true;

    Vector3 lastTargetPos_ = { 0.0f, 0.0f, 0.0f };
};