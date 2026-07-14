#pragma once
#ifdef USE_IMGUI

#include "GpuEmitManager.h"

namespace YoRigine {

// GpuEmitManager のグループ/エミッター編集・保存読込用 ImGui UI。
// USE_IMGUI ガードのため Release ビルドには含まれない。
// GpuEmitManager から責務を分離するために切り出した (神クラス対策)。
// エミッタ形状のギズモ可視化 (DrawEmitterGizmos 等) は Draw() の描画パイプラインと
// 密結合しているため GpuEmitManager 側に残している。
class GpuEmitManagerEditorUI {
public:
	// メニュー・タブ・削除ダイアログを含む全体UI。Editor の GameUI 登録から呼ばれる。
	static void DrawImGui(GpuEmitManager& manager);

private:
	static bool DrawParticleParametersEditor(GpuEmitManager& manager, GpuEmitManager::EmitterData* emitterData);

	static bool DrawShapeEditor(GpuEmitManager& manager, GpuEmitManager::EmitterData* emitterData);
	static bool DrawSphereEditor(GpuEmitManager& manager, GpuEmitManager::EmitterData* emitterData);
	static bool DrawBoxEditor(GpuEmitManager& manager, GpuEmitManager::EmitterData* emitterData);
	static bool DrawTriangleEditor(GpuEmitManager& manager, GpuEmitManager::EmitterData* emitterData);
	static bool DrawConeEditor(GpuEmitManager& manager, GpuEmitManager::EmitterData* emitterData);
	static bool DrawMeshEditor(GpuEmitManager& manager, GpuEmitManager::EmitterData* emitterData);

	static void DrawGroupManagementTab(GpuEmitManager& manager);
	static void DrawEmitterManagementTab(GpuEmitManager& manager);
	static void DrawTextureBrowser(GpuEmitManager& manager, bool& isOpen);
	static void DrawEditorTab(GpuEmitManager& manager);
	static void DrawDeleteDialog(GpuEmitManager& manager);
};

} // namespace YoRigine

#endif
