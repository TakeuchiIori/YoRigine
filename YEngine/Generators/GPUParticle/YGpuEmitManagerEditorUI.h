#pragma once
#ifdef USE_IMGUI

#include "YGpuEmitManager.h"

namespace YoRigine {

// YGpuEmitManager のグループ/エミッター編集・保存読込用 ImGui UI。
// USE_IMGUI ガードのため Release ビルドには含まれない。
// YGpuEmitManager から責務を分離するために切り出した (神クラス対策)。
// エミッタ形状のギズモ可視化 (DrawEmitterGizmos 等) は Draw() の描画パイプラインと
// 密結合しているため YGpuEmitManager 側に残している。
class YGpuEmitManagerEditorUI {
public:
	// メニュー・タブ・削除ダイアログを含む全体UI。Editor の GameUI 登録から呼ばれる。
	static void DrawImGui(YGpuEmitManager& manager);

private:
	static bool DrawParticleParametersEditor(YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData);

	static bool DrawShapeEditor(YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData);
	static bool DrawSphereEditor(YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData);
	static bool DrawBoxEditor(YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData);
	static bool DrawTriangleEditor(YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData);
	static bool DrawConeEditor(YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData);
	static bool DrawMeshEditor(YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData);
	static bool DrawRingEditor(YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData);
	static bool DrawLineEditor(YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData);

	static void DrawGroupManagementTab(YGpuEmitManager& manager);
	static void DrawEmitterManagementTab(YGpuEmitManager& manager);
	static void DrawTextureBrowser(YGpuEmitManager& manager, bool& isOpen);
	static void DrawEditorTab(YGpuEmitManager& manager);
	static void DrawDeleteDialog(YGpuEmitManager& manager);
};

} // namespace YoRigine

#endif
