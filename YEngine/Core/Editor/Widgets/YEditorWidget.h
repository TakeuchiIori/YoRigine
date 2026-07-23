#pragma once
// ===========================================================
// YEditorWidget.h
//
// エディタウィジェット群のアンブレラインクルード。
// 各エディタはこのヘッダ1つをインクルードすれば全機能を使える。
//
// 個別ファイル:
//   Layout  - Section / TreeNode / TabBar / Tab / EditorWindow (RAII)
//   Color   - ColorHDR / ColorHDR3 / Color
//   Value   - DragFloat / SliderFloat / AngleSlider / DragVec3 / DirectionVec3 ...
//   Combo   - Combo / EnumCombo<TEnum>
//   Input   - Checkbox / InputText / InputTextMultiline / InputPath (header-only)
//   Tooltip - ItemTooltip / HelpMarker  (header-only)
//   Scope   - ChangeScope<T> / CommitChange<T>  (header-only, UndoRedo 連携)
// ===========================================================
#ifdef USE_IMGUI

#include "YEditorWidget_Layout.h"
#include "YEditorWidget_Color.h"
#include "YEditorWidget_Value.h"
#include "YEditorWidget_Combo.h"
#include "YEditorWidget_Input.h"
#include "YEditorWidget_Tooltip.h"
#include "YEditorWidget_Scope.h"

#endif // USE_IMGUI
