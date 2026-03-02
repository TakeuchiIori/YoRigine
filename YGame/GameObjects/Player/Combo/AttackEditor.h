#pragma once

#include <vector>
#include <string>
#include <functional>
#include "ComboTypes.h"
#include "AttackDatabase.h"

class AttackDataEditor
{
public:
	AttackDataEditor();

	// 編集対象のリストを設定（省略可、デフォルトは AttackDatabase::Get()）
	void SetTarget(std::vector<AttackData>* list);

	// JSONファイルパスを設定
	void SetFilePath(const std::string& path);

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

	// 操作関数群
	void NewAttack();
	void DuplicateAttack();
	void DeleteAttack();
	void MoveUp();
	void MoveDown();

	// JSON関連
	void LoadFromJson();
	void SaveToJson();

	// リロードをトリガー
	void TriggerReload();

private:
	std::vector<AttackData>* attacks_ = nullptr;
	int currentIndex_ = -1;
	std::string filePath_ = "Resources/Json/Combo/AttackData.json";
	bool isOpen_ = false;
	bool autoReload_ = true;  // デフォルトで自動リロード有効

	char nameBuffer_[256];

	// リロードコールバック
	std::function<void()> onReloadCallback_;
};