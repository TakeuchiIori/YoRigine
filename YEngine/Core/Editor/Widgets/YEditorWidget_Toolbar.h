#pragma once
// ===========================================================
// YEditorWidget_Toolbar.h
//
// ツールバー系ウィジェット。
// ギズモの操作モード切替のような「排他選択ボタン列」と
// 「押しっぱなしトグルボタン」を共通化する。
//
// 使い方:
//   const char* modes[] = { "移動 (W)", "回転 (E)", "拡縮 (R)" };
//   if (YEditorWidget::ToolbarSelector("##gizmoMode", modes, 3, mode)) { ... }
// ===========================================================
#ifdef USE_IMGUI
#include <imgui.h>

namespace YEditorWidget {

// ── 排他選択ボタン列 ──────────────────────────────────────────
// selected が変更されたら true。tooltips は nullptr 可。
bool ToolbarSelector(const char *id, const char *const *labels, int count,
                     int &selected, const char *const *tooltips = nullptr);

// ── トグルボタン ──────────────────────────────────────────────
// 押されている間はハイライト表示になる。変更されたら true。
bool ToolbarToggle(const char *label, bool &value,
                   const char *tooltip = nullptr);

// ── 区切り ────────────────────────────────────────────────────
// 同一行に縦線を引く（ボタン列のグループ分け用）
void ToolbarSeparator();

} // namespace YEditorWidget
#endif // USE_IMGUI
