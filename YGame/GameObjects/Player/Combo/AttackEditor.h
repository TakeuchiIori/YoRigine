#pragma once

#include <vector>
#include <string>
#include <functional>
#include "ComboTypes.h"
#include "AttackDatabase.h"
#include "AttackFrameConverter.h"
#include <Debugger/DopeSheet/DopeSheetEditor.h>

// ============================================================
// 攻撃データエディタクラス
// AttackData の一覧を ImGui で編集するための機能を提供する
//
// [画面レイアウト]
//   上段左 : 攻撃リスト（攻撃タイプ別に折りたたみ表示）
//   上段右 : プロパティインスペクタ（各種数値やフラグの編集）
//   下段   : ドープシート（フレーム単位でのタイムライン視覚編集）
// ============================================================
class AttackDataEditor
{
public:
	// ============================================================
	// 初期化・設定関数
	// ============================================================
	AttackDataEditor();

	void SetTarget(std::vector<AttackData>* list);
	void SetFilePath(const std::string& path);
	void SetReloadCallback(std::function<void()> callback);

	// ============================================================
	// 描画と状態管理
	// ============================================================
	void DrawImGui();

	void SetOpen(bool open) { isOpen_ = open; }
	bool IsOpen() const { return isOpen_; }

	void SetAutoReload(bool enable) { autoReload_ = enable; }
	bool IsAutoReload() const { return autoReload_; }

private:
	// ============================================================
	// 各種UIの描画処理
	// ============================================================
	void DrawToolbar();
	void DrawAttackList();
	void DrawAttackDetail();
	void DrawDopeSheet();

	// ============================================================
	// 攻撃データの操作処理
	// ============================================================
	void NewAttack();
	void DuplicateAttack();
	void DeleteAttack();
	void MoveUp();
	void MoveDown();

	// ============================================================
	// セーブ・ロード・再構築処理
	// ============================================================
	void LoadFromJson();
	void SaveToJson();
	void TriggerReload();
	void OnAttackSelected();

private:
	// ============================================================
	// メンバ変数
	// ============================================================
	std::vector<AttackData>* attacks_ = nullptr;
	int currentIndex_ = -1;
	int prevIndex_ = -1;

	std::string filePath_ = "Resources/Json/Combo/AttackData.json";
	bool isOpen_ = false;
	bool autoReload_ = true;

	char nameBuffer_[256];

	std::function<void()> onReloadCallback_;

	DopeSheet::DopeSheetEditor dopeEditor_;
	std::vector<DopeSheet::DopeTrack> dopeTracks_;
};