#pragma once
#ifdef USE_IMGUI

#include "AttackCameraComponent.h"
#include <string>

class PlayerCamera;

/// <summary>
/// 攻撃カメラワーク ImGui エディター。
/// キーフレームのタイムライン表示・追加・削除・プレビューを提供する。
/// PlayerCamera が所有し、DrawImGui() 内から呼ぶ。
/// </summary>
class AttackCameraEditor {
public:
    // ============================================================
    // セットアップ
    // ============================================================
    void Initialize(AttackCameraComponent* component, PlayerCamera* playerCamera);

    /// メインウィンドウを描画する（PlayerCamera::DrawImGui() から呼ぶ）
    void DrawImGui();

    void SetFilePath(const std::string& path) { filePath_ = path; }
    const std::string& GetFilePath() const { return filePath_; }

    // プレビュー終了通知（PlayerCamera から呼ぶ）
    void NotifyPreviewFinished() { isPreviewPlaying_ = false; }

private:
    // ============================================================
    // UI パネル
    // ============================================================
    void DrawWorkList();
    void DrawKeyframeTimeline();
    void DrawKeyframeInspector();
    void DrawPreviewControls();
    void DrawToolbar();

    // ============================================================
    // 依存
    // ============================================================
    AttackCameraComponent* component_    = nullptr;
    PlayerCamera*          playerCamera_ = nullptr;

    // ============================================================
    // UI 状態
    // ============================================================
    std::string filePath_ = "Resources/Json/Camera/AttackCameraWorks.json";

    int  selectedWorkIdx_  = -1;
    int  selectedKfIdx_    = -1;
    char newNameBuf_[64]   = {};
    bool isPreviewPlaying_ = false;
};

#endif // USE_IMGUI
