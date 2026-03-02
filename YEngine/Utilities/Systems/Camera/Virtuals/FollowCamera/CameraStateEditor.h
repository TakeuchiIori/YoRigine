#pragma once
#include "CameraState.h"
#include <memory>
#include <string>

class FollowCamera;

/// <summary>
/// カメラステートを編集・管理するための統合エディタ
/// </summary>
class CameraStateEditor {
public:
    static CameraStateEditor* GetInstance();
    
    // エディタUIの描画
    void DrawEditorWindow();
    
    // 現在編集中のステートを設定
    void SetEditingState(std::unique_ptr<CameraState> state);
    
    // カメラのセット（プレビュー用）
    void SetCamera(FollowCamera* camera) { camera_ = camera; }
    
private:
    CameraStateEditor() = default;
    ~CameraStateEditor() = default;
    CameraStateEditor(const CameraStateEditor&) = delete;
    CameraStateEditor& operator=(const CameraStateEditor&) = delete;
    
    void DrawStateCreationUI();
    void DrawStateEditorUI();
    void DrawPresetManagerUI();
    void DrawPreviewControlUI();
    
    std::unique_ptr<CameraState> editingState_;
    FollowCamera* camera_ = nullptr;
    
    // UI状態
    int selectedStateType_ = 0;
    char newPresetName_[64] = "";
    bool isPreviewMode_ = false;
};
