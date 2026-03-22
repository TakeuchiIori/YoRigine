#pragma once

#include <vector>
#include <string>
#include <functional>
#include "ComboTypes.h"
#include "AttackDatabase.h"
#include "AttackFrameConverter.h"
#include <Debugger/DopeSheet/DopeSheetEditor.h>

//=============================================================================
// AttackDataEditor
// AttackData の一覧を ImGui で編集するエディタ
//
// 【構成】
//   上段左  : 攻撃リスト（タイプ別折りたたみ）
//   上段右  : プロパティインスペクタ（数値・フラグ類）
//   下段    : ドープシート（フレーム単位のタイムライン編集）
//=============================================================================
class AttackDataEditor
{
public:
    AttackDataEditor();

    // 編集対象のリストを設定（省略可、デフォルトは AttackDatabase::Get()）
    void SetTarget(std::vector<AttackData>* list);

    // JSON ファイルパスを設定
    void SetFilePath(const std::string& path);

    // リロードコールバックを設定（保存後にゲーム側へ通知したいときに使う）
    void SetReloadCallback(std::function<void()> callback);

    // エディタ全体を描画する
    void DrawImGui();

    // 開閉状態
    void SetOpen(bool open) { isOpen_ = open; }
    bool IsOpen()   const { return isOpen_; }

    // 自動リロード（編集のたびに保存 & リロードする）
    void SetAutoReload(bool enable) { autoReload_ = enable; }
    bool IsAutoReload() const { return autoReload_; }

private:
    void DrawToolbar();
    void DrawAttackList();
    void DrawAttackDetail();
    void DrawDopeSheet();

    void NewAttack();
    void DuplicateAttack();
    void DeleteAttack();
    void MoveUp();
    void MoveDown();

    void LoadFromJson();
    void SaveToJson();
    void TriggerReload();

    // 攻撃選択時に BuildTracks を呼んでドープシートを初期化する
    void OnAttackSelected();

private:
    std::vector<AttackData>* attacks_ = nullptr;
    int                      currentIndex_ = -1;
    int                      prevIndex_ = -1;

    std::string filePath_ = "Resources/Json/Combo/AttackData.json";
    bool        isOpen_ = false;
    bool        autoReload_ = true;

    char nameBuffer_[256];

    std::function<void()> onReloadCallback_;

    DopeSheet::DopeSheetEditor        dopeEditor_;
    std::vector<DopeSheet::DopeTrack> dopeTracks_;
};