#include "CollisionEditor.h"

#include "CollisionManager.h"

#include <algorithm>
#include <filesystem>

#ifdef USE_IMGUI
#include "Core/Editor/Editor.h"
#include "Core/Editor/Widgets/YEditorWidget.h"
#include <imgui.h>
#endif

namespace YoRigine {

CollisionEditor* CollisionEditor::GetInstance()
{
	static CollisionEditor instance;
	return &instance;
}

CollisionEditor::CollisionEditor()
{
	autoJson_
		.Add("broadPhaseCellSize", &broadPhaseCellSize_)
		.Add("enableFrustumCulling", &enableFrustumCulling_)
		.Add("resolveIterations", &resolveIterations_)
		.Add("contactExitGraceFrames", &contactExitGraceFrames_);
}

void CollisionEditor::Initialize()
{
	if (initialized_) return;

	initialized_ = true;
	LoadSettings();
}

void CollisionEditor::ApplySettings()
{
	broadPhaseCellSize_ = std::clamp(broadPhaseCellSize_, 0.1f, 100.0f);
	resolveIterations_ = std::clamp(resolveIterations_, 0, 16);
	contactExitGraceFrames_ = std::clamp(contactExitGraceFrames_, 0, 60);

	auto* collisionManager = CollisionManager::GetInstance();
	collisionManager->SetBroadPhaseCellSize(broadPhaseCellSize_);
	collisionManager->SetEnableFrustumCulling(enableFrustumCulling_);
	collisionManager->SetResolveIterations(resolveIterations_);
	collisionManager->SetContactExitGraceFrames(contactExitGraceFrames_);
}

void CollisionEditor::SaveSettings()
{
	ApplySettings();
	autoJson_.SaveToFile(kSettingsPath);
	dirty_ = false;
	status_ = "保存しました: " + std::string(kSettingsPath);
}

void CollisionEditor::LoadSettings()
{
	try {
		if (std::filesystem::exists(kSettingsPath)) {
			autoJson_.LoadFromFile(kSettingsPath);
			status_ = "読み込みました: " + std::string(kSettingsPath);
		}
		else {
			status_ = "設定ファイルがないため既定値を使用しています";
		}
		ApplySettings();
		dirty_ = false;
	}
	catch (const std::exception& e) {
		status_ = "読み込みに失敗しました: " + std::string(e.what());
		ApplySettings();
	}
}

void CollisionEditor::ResetDefaults()
{
	broadPhaseCellSize_ = 2.5f;
	enableFrustumCulling_ = false;
	resolveIterations_ = 3;
	contactExitGraceFrames_ = 2;
	ApplySettings();
	dirty_ = true;
	status_ = "既定値へ戻しました（未保存）";
}

void CollisionEditor::DrawImGui()
{
#ifdef USE_IMGUI
	if (!initialized_) Initialize();

	bool changed = false;

	if (YEditorWidget::Section section{ "BroadPhase グリッド" }) {
		changed |= YEditorWidget::DragFloat(
			"セルサイズ##CollisionEditor",
			broadPhaseCellSize_, 0.1f, 0.1f, 100.0f, "%.2f");
		YEditorWidget::ItemTooltip(
			"当たり判定候補をまとめる3Dグリッドの一辺。\n"
			"小さいほど候補を細かく絞れますが、巨大なコライダーは多くのセルを占有します。");

		changed |= YEditorWidget::Checkbox(
			"視錐台外を候補から除外##CollisionEditor",
			enableFrustumCulling_);
		YEditorWidget::ItemTooltip(
			"IsCheckOutsideCamera が有効なコライダーだけを対象にします。\n"
			"QuerySphere と CCD 用グリッドは画面外も維持されます。");
	}

	if (YEditorWidget::Section section{ "接触解決" }) {
		changed |= YEditorWidget::DragInt(
			"押し戻し反復回数##CollisionEditor",
			resolveIterations_, 1.0f, 0, 16);
		YEditorWidget::ItemTooltip(
			"0で押し戻しなし。通常は3～4回が安定します。");

		changed |= YEditorWidget::DragInt(
			"Exit猶予フレーム##CollisionEditor",
			contactExitGraceFrames_, 1.0f, 0, 60);
		YEditorWidget::ItemTooltip(
			"一時的に離れた接触を継続扱いにするフレーム数。\n"
			"Enter/Exitが細かく繰り返される場合に増やします。");
	}

	if (changed) {
		ApplySettings();
		dirty_ = true;
		status_ = "変更を適用しました（未保存）";
	}

	if (YEditorWidget::Section section{ "実行状況" }) {
		const auto* collisionManager = CollisionManager::GetInstance();
		ImGui::Text("有効コライダー: %zu",
			collisionManager->GetLastActiveColliderCount());
		ImGui::Text("BroadPhase候補ペア: %zu",
			collisionManager->GetLastBroadPhasePairCount());
		ImGui::Text("NarrowPhaseヒット: %zu",
			collisionManager->GetLastNarrowPhaseHitCount());
		ImGui::Text("CCD候補: %zu",
			collisionManager->GetLastCCDCandidateCount());
		ImGui::Text("範囲検索候補: %zu",
			collisionManager->GetLastQuerySphereCandidateCount());
	}

	YEditorWidget::SectionHeader("保存・読み込み");
	if (Editor::Button("設定を保存")) {
		SaveSettings();
	}
	ImGui::SameLine();
	if (ImGui::Button("再読み込み")) {
		LoadSettings();
	}
	ImGui::SameLine();
	if (ImGui::Button("既定値に戻す")) {
		ResetDefaults();
	}

	ImGui::Separator();
	if (dirty_) {
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "未保存の変更があります");
	}
	ImGui::TextWrapped("%s", status_.c_str());
	ImGui::TextDisabled("%s", kSettingsPath);
#endif
}

} // namespace YoRigine
