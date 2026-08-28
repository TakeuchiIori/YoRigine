#pragma once
// ===========================================================
// YEditorWidget_Hierarchy.h
//
// アウトライナ (階層リスト) の 1 行を描くウィジェット。
// Unity の Hierarchy / Unreal の World Outliner と同じく
//   [表示トグル] [ロックトグル] 名前
// の並びを共通化する。ツリー構造の走査自体は呼び出し側の責務で、
// ここは「1 行の見た目と入力処理」だけを持つ。
//
// 使い方:
//   YEditorWidget::HierarchyRow row;
//   row.indent = depth;
//   auto result = YEditorWidget::DrawHierarchyRow(id, label, selected, row);
//   if (result.clicked) { select(id, result.additive); }
// ===========================================================
#ifdef USE_IMGUI
#include <imgui.h>

namespace YEditorWidget {

// 行に表示するトグル。nullptr を渡すとそのトグルは描画されない。
struct HierarchyRow {
  bool *visible = nullptr; // 表示 / 非表示 (目のアイコン相当)
  bool *locked = nullptr;  // 選択ロック (南京錠アイコン相当)
  int indent = 0;          // 階層の深さ
  bool hasChildren = false;
  bool *expanded = nullptr; // 折りたたみ状態 (hasChildren のとき使う)
};

struct HierarchyRowResult {
  bool clicked = false;          // 行が選択された
  bool additive = false;         // Ctrl 併用の追加選択
  bool doubleClicked = false;    // ダブルクリック (フォーカス移動など)
  bool toggledVisible = false;   // 表示トグルが押された
  bool toggledLock = false;      // ロックトグルが押された
  bool toggledExpand = false;    // 折りたたみが切り替わった
  bool requestDelete = false;    // 右クリックメニューから削除が選ばれた
  bool requestRename = false;    // 右クリックメニューから名前変更が選ばれた
  bool requestDuplicate = false; // 右クリックメニューから複製が選ばれた
};

// 1 行を描画する。id は ImGui の PushID 用に一意な値を渡すこと。
HierarchyRowResult DrawHierarchyRow(int id, const char *label, bool selected,
                                    const HierarchyRow &row);

} // namespace YEditorWidget
#endif // USE_IMGUI
