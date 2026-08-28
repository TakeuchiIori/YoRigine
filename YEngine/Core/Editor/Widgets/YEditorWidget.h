#pragma once
// ===========================================================
// YEditorWidget.h
//
// エディタウィジェット群のアンブレラインクルード。
// 各エディタはこのヘッダ1つをインクルードすれば全機能を使える。
//
// 個別ファイル:
//   Layout      - Section / TreeNode / TabBar / Tab / EditorWindow (RAII)
//   Color       - ColorHDR / ColorHDR3 / Color
//   Value       - DragFloat / SliderFloat / AngleSlider / DragVec3 /
//   DirectionVec3 ... Combo       - Combo / EnumCombo<TEnum> Input       -
//   Checkbox / InputText / InputTextMultiline / InputPath (header-only) Tooltip
//   - ItemTooltip / HelpMarker  (header-only) Scope       - ChangeScope<T> /
//   CommitChange<T>  (header-only, UndoRedo 連携) Transform   - TransformFields
//   / DragEulerDegrees Material    - MaterialSlotEditor / OverrideFloat /
//   OverrideColor3 AssetPicker - SearchBox / AssetCombo / AssetPickerPopup /
//   ScanAssetFiles Hierarchy   - DrawHierarchyRow (アウトライナの 1 行) Toolbar
//   - ToolbarSelector / ToolbarToggle / ToolbarSeparator
// ===========================================================
#ifdef USE_IMGUI

#include "YEditorWidget_AssetPicker.h"
#include "YEditorWidget_Color.h"
#include "YEditorWidget_Combo.h"
#include "YEditorWidget_Hierarchy.h"
#include "YEditorWidget_Input.h"
#include "YEditorWidget_Layout.h"
#include "YEditorWidget_Material.h"
#include "YEditorWidget_Scope.h"
#include "YEditorWidget_Toolbar.h"
#include "YEditorWidget_Tooltip.h"
#include "YEditorWidget_Transform.h"
#include "YEditorWidget_Value.h"

#endif // USE_IMGUI
