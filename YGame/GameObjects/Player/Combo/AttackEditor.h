#pragma once

#include <vector>
#include <string>
#include <functional>
#include "ComboTypes.h"
#include "AttackDatabase.h"
#include "AttackFrameData.h"
#include "AttackFrameDatabase.h"
#include "AttackFrameConverter.h"
#include <Debugger/DopeSheet/DopeSheetEditor.h>

class AttackDataEditor
{
public:
    AttackDataEditor();

    // 編集対象のリストを設定（省略可、デフォルトは AttackDatabase::Get()）
    void SetTarget(std::vector<AttackData>* list);

    // JSONファイルパスを設定
    void SetFilePath(const std::string& path);

    // フレームデータ用JSONファイルパスを設定
    void SetFrameFilePath(const std::string& path);

    // リロードコールバックを設定（自動リロード時に呼ばれる）
    void SetReloadCallback(std::function<void()> callback);

    // エディターウィンドウを表示
    void DrawImGui();

    // エディターの開閉状態
    void SetOpen(bool open) { isOpen_ = open; }
    bool IsOpen() const { return isOpen_; }

    // 自動リロード設定
    void SetAutoReload(bool enable) { autoReload_ = enable; }
    bool IsAutoReload() const { return autoReload_; }

private:
    // UI描画関数群
    void DrawToolbar();
    void DrawAttackList();
    void DrawAttackDetail();
    void DrawDopeSheet();           // ★ 追加：ドープシートエリア

    // 操作関数群
    void NewAttack();
    void DuplicateAttack();
    void DeleteAttack();
    void MoveUp();
    void MoveDown();

    // JSON関連
    void LoadFromJson();
    void SaveToJson();

    // フレームデータ JSON関連
    void LoadFrameDataFromJson();
    void SaveFrameDataToJson();

    // リロードをトリガー
    void TriggerReload();

    // ドープシート同期
    void OnAttackSelected();        // ★ 攻撃選択時に BuildTracks を呼ぶ
    AttackFrameData& GetOrCreateFrameData(); // ★ 現在選択中の FrameData を取得/生成

private:
    // ── 既存メンバ ──
    std::vector<AttackData>* attacks_ = nullptr;
    int  currentIndex_  = -1;
    int  prevIndex_     = -1;      // ★ 選択変化の検知用
    std::string filePath_      = "Resources/Json/Combo/AttackData.json";
    std::string frameFilePath_ = "Resources/Json/Combo/AttackFrameData.json"; // ★
    bool isOpen_     = false;
    bool autoReload_ = true;

    char nameBuffer_[256];

    std::function<void()> onReloadCallback_;

    // ── 追加メンバ ──
    DopeSheet::DopeSheetEditor          dopeEditor_;   // ★
    std::vector<DopeSheet::DopeTrack>   dopeTracks_;   // ★ 現在選択中の攻撃分
};
