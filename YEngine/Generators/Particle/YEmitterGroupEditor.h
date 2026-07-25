#pragma once
#ifdef USE_IMGUI

#include "YEmitterGroupManager.h"
#include "FileOperations/FileBrowser.h"
#include "Debugger/Gizmo/GizmoController.h"
#include "YEmitterGizmable.h"
#include "YEmitterGroupGizmable.h"

#include <string>
#include <vector>
#include <memory>
#include <set>

/// <summary>
/// YEmitterGroup の ImGui エディタ（ギズモ統合版）
///
/// ■ ギズモ操作の流れ
///   1. グループ行をクリック        → グループ全体 (YEmitterGroupGizmable) をギズモ対象に
///   2. エミッター行の [G] ボタン   → 個別エミッター (YEmitterGizmable) をギズモ対象に
///   3. Ctrl + [G] ボタン           → 複数エミッター同時選択 → 差分移動
///   4. DrawGizmo() をビューポート後に呼ぶとギズモが描画される
///
/// ■ 呼び出し方（例: YParticleEditor）
///   void YParticleEditor::Update() {
///       auto& editor = YEmitterGroupEditor::GetInstance();
///       editor.SetCamera(camera_);
///       editor.SetViewport(viewportPos, viewportSize);
///   }
///   void YParticleEditor::ShowEditorTab() {
///       editor.ShowEditor();   // ImGui ウィンドウ内
///   }
///   void YParticleEditor::DrawGizmo() {
///       editor.DrawGizmo();    // ImGui::Image の直後
///   }
/// </summary>
class YEmitterGroupEditor {
public:
    //=================================================================
    // シングルトン
    //=================================================================

    static YEmitterGroupEditor& GetInstance() {
        static YEmitterGroupEditor instance;
        return instance;
    }

    YEmitterGroupEditor(const YEmitterGroupEditor&) = delete;
    YEmitterGroupEditor& operator=(const YEmitterGroupEditor&) = delete;

    //=================================================================
    // セットアップ（毎フレームまたは初期化時）
    //=================================================================

    void SetCamera(YoRigine::Camera* camera) { camera_ = camera; }

    //=================================================================
    // 描画
    //=================================================================

    /// ImGui ウィンドウ内（タブ内）で呼ぶ
    void ShowEditor();

    /// ImGui::Image の直後（同フレーム・同ウィンドウ内）で呼ぶ
    void DrawGizmo();

private:
    YEmitterGroupEditor();
    ~YEmitterGroupEditor() = default;

    //=================================================================
    // 内部描画
    //=================================================================

    void ShowGizmoToolbar();
    void ShowGroupList();
    void ShowGroupDetail();
    bool ShowEmitterRow(YEmitterGroup& group, size_t index);
    void ShowCreateGroupUI();
    void ShowFileButtons();
    void ShowSelectedEmitterDetail(YParticleEmitter& emitter);

    /// <summary>
    /// 保存/読み込み完了トーストを描画（タイマーが残っている間だけ表示）
    /// </summary>
    void ShowSaveNotification();

    /// <summary>
    /// 通知をセット
    /// </summary>
    void SetNotification(bool success, const std::string& message);

    //=================================================================
    // ギズモ選択
    //=================================================================

    // グループ全体を選択（グループギズモ）
    void SelectGroup(const std::string& groupName);

    // エミッターを選択（multiSelect=true で Ctrl+クリック）
    void SelectEmitter(const std::string& groupName, int index, bool multiSelect);

    // 選択状態から gizmoTargets_ を再構築する
    void RebuildGizmoTargets();

    //=================================================================
    // 選択モード
    //=================================================================

    enum class SelectionMode { None, Group, Emitter };
    SelectionMode selectionMode_ = SelectionMode::None;

    std::string   selectedGroupName_;
    std::set<int> selectedEmitterIndices_;  // 複数選択対応
    int           selectedEmitterIndex_ = -1;  // Detail 表示用（最後に選択されたもの）

    //=================================================================
    // ギズモ
    //=================================================================

    GizmoController gizmoCtrl_;
    YoRigine::Camera* camera_ = nullptr;

    // アダプタ（毎回 RebuildGizmoTargets() で作り直す）
    std::unique_ptr<YEmitterGroupGizmable>         groupGizmable_;
    std::vector<std::unique_ptr<YEmitterGizmable>> emitterGizmables_;
    std::vector<IGizmable*>                        gizmoTargets_;

    //=================================================================
    // UI 状態
    //=================================================================

    char  newGroupNameBuf_[128] = "";
    char  newEmitterSystemName_[128] = "";
    float newEmitterOffset_[3] = { 0, 0, 0 };
    float groupListWidth_ = 180.0f;

    // プレビュー再生位置。保存位置が画面外でも確認できるよう、ここで指定した
    // 見える位置にエフェクト/システムを発生させる。
    float previewPos_[3] = { 0.0f, 2.0f, 0.0f };

    const std::string emitterDirectory_ = "Resources/Json/YEmitterGroups/";

    //=================================================================
    // FileBrowser
    //=================================================================

    YoRigine::FileBrowser saveBrowser_;
    YoRigine::FileBrowser loadBrowser_;
    bool showSavePopup_ = false;
    bool showLoadPopup_ = false;

    // 個別グループ保存/ロード
    YoRigine::FileBrowser saveSingleBrowser_;
    YoRigine::FileBrowser loadSingleBrowser_;
    bool showSaveSinglePopup_ = false;
    bool showLoadSinglePopup_ = false;

    // バンドル保存/読込（YEffects/ = 1エフェクト=1ファイル）
    YoRigine::FileBrowser saveBundleBrowser_;
    bool showSaveBundlePopup_ = false;
    YoRigine::FileBrowser loadBundleBrowser_;
    bool showLoadBundlePopup_ = false;

    char saveAsNameBuf_[256] = "";

    //=================================================================
    // 保存完了トースト通知
    //=================================================================

    float       saveNotifyTimer_ = 0.0f;   // 残り表示時間（秒）
    bool        saveNotifySuccess_ = true;    // true=成功 / false=失敗
    std::string saveNotifyMessage_;           // 表示するメッセージ文字列
};

#endif // USE_IMGUI