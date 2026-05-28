#pragma once
#ifdef USE_IMGUI

#include <string>

// エリアの作成・編集・削除・JSON 保存読み込みを行う独自エディタウィンドウ
// 毎フレーム Update() を呼ぶだけで表示される
//
// 使い方:
//   #ifdef USE_IMGUI
//   AreaEditor::GetInstance()->Update();
//   #endif
class AreaEditor
{
public:
	static AreaEditor* GetInstance();

	// 毎フレーム呼ぶ
	void Update();

private:
	AreaEditor() = default;
	~AreaEditor() = default;
	AreaEditor(const AreaEditor&) = delete;
	AreaEditor& operator=(const AreaEditor&) = delete;

	// ---- ペイン描画 ----
	void DrawAreaList();
	void DrawAreaProperties();
	void DrawFilePanel();

	// ---- 形状固有の拡張 UI ----
	void DrawPolygonProperties(class PolygonArea* poly);

	// ---- 追加モーダル ----
	void DrawAddModal();

	// ---- 内部ヘルパー ----
	void ApplyRename(const std::string& newName);

private:
	std::string selectedName_;

	// ファイルパス
	char filePathBuf_[256] = "Resources/Json/Areas/areas.json";

	// 新規追加モーダル用
	bool showAddModal_   = false;
	char newNameBuf_[64] = "NewArea";
	int  newTypeIndex_   = 0; // 0=Circle, 1=Polygon

	// リネーム用
	bool isRenaming_     = false;
	char renameBuf_[64]  = "";
};

#endif // USE_IMGUI
