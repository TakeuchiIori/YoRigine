#include "MotionEditor.h"

#include "Systems/Camera/Camera.h"
#include "Model.h"
#include "../Core/MotionSystem.h"
#include "Skeleton/Joint.h"
#include "Skeleton/Skeleton.h"
#include "Skeleton/BoneGizmable.h"
#include <json.hpp>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <cmath>
#include <sstream>
#include <iomanip>

#include <Editor/Icon/EditorIcon.h>
#include "Object3D/ObjectManager.h" 
#include <Editor/Editor.h>

// パネルのインクルード
#include "Panels/ToolbarPanel.h"
#include "Panels/BoneListPanel.h"
#include "Panels/PropertyPanel.h"
#include "Panels/TimelinePanel.h"

#ifdef USE_IMGUI
#include "ImGuizmo.h"
#endif

namespace fs = std::filesystem;

// ============================================================
// Context Helper Implementation
// ============================================================
Object3d* MotionEditorContext::GetTargetObject() const {
	if (targetObjectId != -1) {
		return ObjectManager::GetInstance()->GetObject3dById(targetObjectId);
	}
	return nullptr;
}

MotionEditor::MotionEditor() {}
MotionEditor::~MotionEditor() {}

// ============================================================
// 初期化
// ============================================================
void MotionEditor::Initialize(Camera* camera)
{
	context_.camera = camera;
	previewTransform_.Initialize();
	binaryBrowser_.currentDirectory = "Resources/Binary";
	binaryBrowser_.filterExtension = ".anim";
	lineDrawer_ = std::make_unique<Line>();
	lineDrawer_->SetCamera(context_.camera);
	lineDrawer_->Initialize();
	lineDrawer_->SetColor({ 0.5f, 0.5f, 0.5f, 1.0f });

#ifdef USE_IMGUI
	boneObj_ = Object3d::Create("ICO.obj");
	gizmoCtrl_.Initialize();
	gizmoCtrl_.SetMode(GizmoController::Mode::Local);
#endif

	// ============================================================
	// コンテキストのコールバック設定 (UIとロジックの分離)
	// ============================================================
	context_.SyncJointToBuffer = [this]() { SyncJointToBuffer(context_.selBone); };
	context_.SyncBufferToJoint = [this]() { SyncBufferToJoint(); };

	// KF追加の実処理
	context_.AddKeyframe = [this](const std::string& bone, float time) {
		if (!context_.currentMotion) return;
		auto& nodeAnims = context_.currentMotion->animation_.nodeAnimations_;
		if (!nodeAnims.count(bone)) return;

		Vector3 newT = { context_.editT[0], context_.editT[1], context_.editT[2] };
		Quaternion newR = EulerToQuaternion({ context_.editR[0] * (kPi / 180), context_.editR[1] * (kPi / 180), context_.editR[2] * (kPi / 180) });
		Vector3 newS = { context_.editS[0], context_.editS[1], context_.editS[2] };
		Motion::NodeAnimation snap = nodeAnims[bone];

		context_.history.Execute(MakeLambdaCommand("KF挿入: " + bone,
			[this, bone, time, newT, newR, newS]() {
				auto& na = context_.currentMotion->animation_.nodeAnimations_[bone];
				auto insertOrReplace = [&]<typename T>(auto& kfs, T val) {
					using KF = typename std::remove_reference<decltype(kfs)>::type::value_type;
					auto it = std::find_if(kfs.begin(), kfs.end(),
						[time](const KF& k) { return std::abs(k.time - time) < 1e-4f; });
					if (it != kfs.end()) { it->value = val; }
					else {
						KF k; k.time = time; k.value = val; kfs.push_back(k);
						std::sort(kfs.begin(), kfs.end(), [](const KF& a, const KF& b) { return a.time < b.time; });
					}
				};
				insertOrReplace(na.translate.keyframes, newT);
				insertOrReplace(na.rotate.keyframes, newR);
				insertOrReplace(na.scale.keyframes, newS);
				context_.requireTimelineRebuild = true;
			},
			[this, bone, snap]() {
				context_.currentMotion->animation_.nodeAnimations_[bone] = snap;
				context_.requireTimelineRebuild = true;
			}
		));
		};

	// KF削除の実処理
	context_.DeleteKeyframe = [this](const std::string& bone, float time) {
		if (!context_.currentMotion) return;
		auto& nodeAnims = context_.currentMotion->animation_.nodeAnimations_;
		if (!nodeAnims.count(bone)) return;

		Motion::NodeAnimation snap = nodeAnims[bone];
		context_.history.Execute(MakeLambdaCommand("KF削除: " + bone,
			[this, bone, time]() {
				auto& na = context_.currentMotion->animation_.nodeAnimations_[bone];
				auto rem = [time](auto& kfs) {
					kfs.erase(std::remove_if(kfs.begin(), kfs.end(),
						[time](const auto& k) { return std::abs(k.time - time) < 1e-4f; }), kfs.end());
					};
				rem(na.translate.keyframes);
				rem(na.rotate.keyframes);
				rem(na.scale.keyframes);
				context_.requireTimelineRebuild = true;
			},
			[this, bone, snap]() {
				context_.currentMotion->animation_.nodeAnimations_[bone] = snap;
				context_.requireTimelineRebuild = true;
			}
		));
		};

	// ============================================================
	// パネルの登録
	// ============================================================
	RegisterPanel(std::make_unique<ToolbarPanel>());
	RegisterPanel(std::make_unique<BoneListPanel>());
	RegisterPanel(std::make_unique<PropertyPanel>());
	RegisterPanel(std::make_unique<TimelinePanel>());
}

void MotionEditor::RegisterPanel(std::unique_ptr<IMotionEditorPanel> panel)
{
	panel->Initialize(&context_);
	panels_.push_back(std::move(panel));
}

// ============================================================
// 更新
// ============================================================
void MotionEditor::Update()
{
	Object3d* target = context_.GetTargetObject();
	if (!target) return;

	Model* m = target->GetModel();
	MotionSystem* ms = m ? m->GetMotionSystem() : nullptr;

	if (ms) {
		float msTime = ms->GetAnimationTime();
		if (std::abs(msTime - context_.scrubTime) > 1e-4f) {
			context_.scrubTime = msTime;
			// 同期はTimelinePanel内で処理されるためここでは時間更新のみ
		}
	}

	if (m && m->GetSkeleton()) {
		if (ms && !context_.isPlaying && context_.currentMotion) {
			context_.currentMotion->ApplyAnimation(m->GetSkeleton()->GetJoints(), context_.scrubTime);

			if (!context_.selBone.empty()) {
#ifdef USE_IMGUI
				if (draggingBone_ || gizmoCtrl_.IsUsing()) {
					SyncBufferToJoint();
				}
				else {
					SyncJointToBuffer(context_.selBone);
				}
#else
				SyncJointToBuffer(context_.selBone);
#endif
			}
		}

		m->GetSkeleton()->Update();
		if (m->GetSkinCluster()) {
			m->GetSkinCluster()->UpdateMatrixPalette(m->GetSkeleton()->GetJoints());
		}
	}

	previewTransform_.UpdateMatrix();

	// 各パネルの更新
	for (auto& panel : panels_) {
		if (panel->IsActive()) panel->Update();
	}
}

// ============================================================
// エディタの描画
// ============================================================
void MotionEditor::ShowEditor()
{
#ifdef USE_IMGUI
	if (binaryBrowser_.isOpen) {
		DrawFileBrowser(binaryBrowser_, "バイナリファイルを選択");
		if (!binaryBrowser_.isOpen && !binaryBrowser_.selectedFilePath.empty())
			savePath_ = binaryBrowser_.selectedFilePath;
	}

	DrawSaveLoadPopup();

	ImGui::SetNextWindowSize(ImVec2(1200, 750), ImGuiCond_FirstUseEver);
	ImGui::Begin("Motion Editor", nullptr, ImGuiWindowFlags_MenuBar);

	Object3d* target = context_.GetTargetObject();

	// ---------------- ボーン選択＆ギズモ処理 ----------------
	if (context_.isDrawBone && target && target->GetModel() && target->GetModel()->GetSkeleton()) {
		if (ImGui::IsMouseClicked(0) && !ImGuizmo::IsOver() && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
			gizmoCtrl_.ClearPickables();
			boneGizmables_.clear();

			auto& joints = target->GetModel()->GetSkeleton()->GetJoints();
			for (const auto& joint : joints) {
				auto bg = std::make_unique<BoneGizmable>(this, joint.GetName());
				bg->UpdateFromJoint();
				gizmoCtrl_.RegisterPickable(bg.get());
				boneGizmables_.push_back(std::move(bg));
			}

			ImVec2 viewPos = Editor::GetInstance()->GetGameViewPos();
			ImVec2 viewSize = Editor::GetInstance()->GetGameViewSize();

			IGizmable* picked = gizmoCtrl_.TryPickObject(ImGui::GetMousePos(), viewPos, viewSize, context_.camera);
			if (picked) {
				BoneGizmable* bg = static_cast<BoneGizmable*>(picked);
				context_.selBone = bg->GetBoneName();
				SyncJointToBuffer(context_.selBone);
				context_.statusMsg = "ボーン選択: " + context_.selBone;
			}
		}

		if (!context_.selBone.empty()) {
			static BoneGizmable currentSelectedGizmo(this, "");
			if (currentSelectedGizmo.GetBoneName() != context_.selBone) {
				currentSelectedGizmo = BoneGizmable(this, context_.selBone);
			}
			currentSelectedGizmo.UpdateFromJoint();

			std::vector<IGizmable*> gizmoTargets = { &currentSelectedGizmo };
			ImVec2 viewPos = Editor::GetInstance()->GetGameViewPos();
			ImVec2 viewSize = Editor::GetInstance()->GetGameViewSize();
			gizmoCtrl_.Draw(context_.camera, gizmoTargets, viewPos, viewSize);
		}
	}

	// フォーカス中のキー入力
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
		context_.history.HandleKeyInput();

		if (ImGui::IsKeyPressed(ImGuiKey_Space) && target) {
			if (context_.isPlaying) {
				if (target->GetModel() && target->GetModel()->GetMotionSystem()) target->GetModel()->GetMotionSystem()->Stop();
				context_.isPlaying = false;
				context_.statusMsg = "[ Space ] 一時停止";
			}
			else {
				if (target->GetModel() && target->GetModel()->GetMotionSystem()) {
					auto* ms = target->GetModel()->GetMotionSystem();
					float savedTime = ms->GetAnimationTime();
					if (savedTime >= ms->GetDuration() || ms->IsFinished()) savedTime = 0.0f;
					if (context_.isLoop) target->PlayLoop(); else target->PlayOnce();
					ms->SetAnimationTime(savedTime);
				}
				context_.isPlaying = true;
				context_.statusMsg = "[ Space ] 再生";
			}
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Delete) && context_.selKF.IsValid() && context_.currentMotion) {
			if (context_.DeleteKeyframe) {
				auto& na = context_.currentMotion->animation_.nodeAnimations_[context_.selKF.boneName];
				float t = 0;
				if (context_.selKF.index < (int)na.translate.keyframes.size())
					t = na.translate.keyframes[context_.selKF.index].time;
				context_.DeleteKeyframe(context_.selKF.boneName, t);
			}
			context_.selKF.Clear();
			context_.statusMsg = "[ Del ] キーフレーム削除";
		}
	}

	DrawMenuBar();

	if (!target || !target->GetModel()) {
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 40);
		float w = ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorPosX((w - 220) * 0.5f);
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "シーンエディタでモデルを選択してください");
		ImGui::End();
		return;
	}

	// 登録されたパネルを順に描画
	for (auto& panel : panels_) {
		if (panel->IsActive()) {
			panel->DrawImGui();
		}
	}

	DrawStatusBar();
	ImGui::End();
#endif
}

// ============================================================
// その他 描画・機能実装群 (既存コードを修正し維持)
// ============================================================
void MotionEditor::Draw()
{
	Object3d* target = context_.GetTargetObject();
	if (context_.isDrawBone && target) {
#ifdef USE_IMGUI
		if (boneObj_ && target->GetModel() && target->GetModel()->GetSkeleton()) {
			auto& joints = target->GetModel()->GetSkeleton()->GetJoints();
			if (boneWorldTransforms_.size() != joints.size()) {
				boneWorldTransforms_.resize(joints.size());
				for (auto& wt : boneWorldTransforms_) wt.Initialize();
			}

			for (size_t i = 0; i < joints.size(); ++i) {
				auto& wt = boneWorldTransforms_[i];
				Matrix4x4 targetMat = GetTargetWorldMatrix();
				Matrix4x4 wMat = joints[i].GetWorldTransform().matWorld_ * targetMat;

				float t[3], rDeg[3], s[3];
				ImGuizmo::DecomposeMatrixToComponents(&wMat.m[0][0], t, rDeg, s);

				wt.translate_ = { t[0], t[1], t[2] };
				wt.rotate_ = { rDeg[0] * (kPi / 180.f), rDeg[1] * (kPi / 180.f), rDeg[2] * (kPi / 180.f) };
				wt.scale_ = { 0.1f, 0.1f, 0.1f };
				wt.UpdateMatrix();

				if (context_.selBone == joints[i].GetName()) boneObj_->SetMaterialColor({ 1.0f, 0.85f, 0.2f, 1.0f });
				else boneObj_->SetMaterialColor({ 0.5f, 0.5f, 0.5f, 1.0f });

				boneObj_->Draw(context_.camera, wt);
			}
		}
#endif
	}
}

void MotionEditor::DrawGizmo()
{
#ifdef USE_IMGUI
	Object3d* target = context_.GetTargetObject();
	if (context_.isDrawBone && target && target->GetModel() && target->GetModel()->GetSkeleton()) {
		if (!context_.selBone.empty()) {
			static BoneGizmable currentSelectedGizmo(this, "");
			if (currentSelectedGizmo.GetBoneName() != context_.selBone) {
				currentSelectedGizmo = BoneGizmable(this, context_.selBone);
			}
			if (!gizmoCtrl_.IsUsing()) currentSelectedGizmo.UpdateFromJoint();

			std::vector<IGizmable*> gizmoTargets = { &currentSelectedGizmo };
			ImVec2 viewPos = Editor::GetInstance()->GetGameViewPos();
			ImVec2 viewSize = Editor::GetInstance()->GetGameViewSize();
			gizmoCtrl_.Draw(context_.camera, gizmoTargets, viewPos, viewSize);
		}
	}
#endif
}

void MotionEditor::DrawBone()
{
	Object3d* target = context_.GetTargetObject();
	if (context_.isDrawBone && target) {
		lineDrawer_->SetCamera(context_.camera);
		target->DrawBone(*lineDrawer_.get(), GetTargetWorldMatrix());
	}
}

void MotionEditor::SetTargetObjectId(int id) {
	Object3d* obj = nullptr;
	if (id != -1) {
		obj = ObjectManager::GetInstance()->GetObject3dById(id);
		if (obj && obj->GetModel()) {
			if (obj->GetModel()->GetMotionSystem() == nullptr) id = -1;
		}
		else id = -1;
	}

	if (context_.targetObjectId != id) {
		context_.targetObjectId = id;

		if (context_.targetObjectId != -1) {
			Object3d* target = context_.GetTargetObject();
			Model* model = target->GetModel();
			MotionSystem* ms = model->GetMotionSystem();

			loadFileName_ = model->GetName();
			context_.currentMotion = ms->GetAnimation();
			selectedAnimKey_ = "";

			if (context_.currentMotion) {
				for (auto& [key, motion] : Model::animationCache_) {
					if (&motion == context_.currentMotion) {
						selectedAnimKey_ = key;
						break;
					}
				}
				context_.requireTimelineRebuild = true;
			}

			context_.scrubTime = ms->GetAnimationTime();
			context_.isPlaying = true;
			context_.statusMsg = "対象オブジェクトを同期: " + loadFileName_;
		}
		else {
			context_.currentMotion = nullptr;
			selectedAnimKey_ = "";
			context_.selBone = "";
			context_.selKF.Clear();
			loadFileName_ = "";
			context_.statusMsg = "Ready";
		}
	}
}

void MotionEditor::DrawMenuBar()
{
#ifdef USE_IMGUI
	if (!ImGui::BeginMenuBar()) return;

	if (ImGui::BeginMenu("ファイル (File)")) {
		if (ImGui::MenuItem("バイナリ保存/読込...", ""))  context_.showSavePopup = true;
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("編集 (Edit)")) {
		if (ImGui::MenuItem("元に戻す", "Ctrl+Z", false, context_.history.CanUndo())) context_.history.Undo();
		if (ImGui::MenuItem("やり直す", "Ctrl+Y", false, context_.history.CanRedo())) context_.history.Redo();
		ImGui::Separator();
		if (ImGui::MenuItem("履歴をクリア")) context_.history.Clear();
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("表示 (View)")) {
		ImGui::SliderFloat("タイムライン高さ", &timelineH_, 80.0f, 500.0f, "%.0f px");
		ImGui::SliderFloat("ボーンリスト幅", &bonePanelW_, 80.0f, 450.0f, "%.0f px");
		ImGui::SliderFloat("ズーム (px/秒)", &timelineZoom_, 20.0f, 400.0f, "%.0f");
		ImGui::EndMenu();
	}

	ImGui::Separator();
	if (!context_.history.CanUndo()) ImGui::BeginDisabled();
	if (ImGui::SmallButton(" << ")) { context_.history.Undo(); context_.statusMsg = "元に戻す"; }
	if (!context_.history.CanUndo()) ImGui::EndDisabled();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("元に戻す (Ctrl+Z)");

	ImGui::SameLine(0, 2);
	if (!context_.history.CanRedo()) ImGui::BeginDisabled();
	if (ImGui::SmallButton(" >> ")) { context_.history.Redo(); context_.statusMsg = "やり直す"; }
	if (!context_.history.CanRedo()) ImGui::EndDisabled();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("やり直す (Ctrl+Y)");

	ImGui::EndMenuBar();
#endif
}

void MotionEditor::DrawStatusBar()
{
#ifdef USE_IMGUI
	float barH = 20.0f;
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
	ImGui::BeginChild("##status", ImVec2(0, barH), false);

	ImGui::SetCursorPosY(3);
	ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "  %s", context_.statusMsg.c_str());

	float hints_x = ImGui::GetContentRegionAvail().x - 400;
	if (hints_x > 200) {
		ImGui::SameLine(hints_x);
		ImGui::TextDisabled("Space=再生  Del=KF削除  Ctrl+Z=元に戻す  Ctrl+Y=やり直す");
	}

	ImGui::EndChild();
	ImGui::PopStyleColor();
#endif
}

void MotionEditor::DrawSaveLoadPopup()
{
#ifdef USE_IMGUI
	if (context_.showSavePopup)
		ImGui::OpenPopup("保存 / 読み込み##popup");

	ImGui::SetNextWindowSize(ImVec2(500, 280), ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal("保存 / 読み込み##popup", &context_.showSavePopup)) {

		ImGui::Text("バイナリファイル (.anim):");
		{
			char buf[512]; strncpy_s(buf, sizeof(buf), savePath_.c_str(), _TRUNCATE);
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90);
			if (ImGui::InputText("##savepath", buf, sizeof(buf))) savePath_ = buf;
			ImGui::SameLine();
			if (ImGui::Button("参照##b")) {
				binaryBrowser_.isOpen = true;
				binaryBrowser_.selectedFilePath = "";
				context_.showSavePopup = false;
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		bool canSave = context_.currentMotion != nullptr;
		if (!canSave) ImGui::BeginDisabled();
		if (ImGui::Button("バイナリ保存", ImVec2(130, 0))) {
			try {
				if (context_.currentMotion) {
					context_.currentMotion->SaveBinary(*context_.currentMotion, AnimDisplayName(selectedAnimKey_), savePath_);
					saveMsg_ = "保存成功: " + savePath_;
					context_.statusMsg = saveMsg_;
				}
			}
			catch (const std::exception& e) {
				saveMsg_ = std::string("保存失敗: ") + e.what();
			}
		}
		if (!canSave) ImGui::EndDisabled();

		ImGui::SameLine(0, 10);
		Object3d* target = context_.GetTargetObject();

		if (ImGui::Button("バイナリ読み込み", ImVec2(140, 0))) {
			try {
				Motion loaded = Motion().LoadBinary(savePath_);
				const std::string key = "Binary:" + savePath_;
				Model::animationCache_[key] = std::move(loaded);
				selectedAnimKey_ = key;
				context_.currentMotion = &Model::animationCache_[key];
				if (target) {
					target->SetChangeMotion(loadFileName_, MotionPlayMode::Loop, AnimDisplayName(key));
				}
				saveMsg_ = "読み込み成功: " + savePath_;
				context_.statusMsg = saveMsg_;
				context_.requireTimelineRebuild = true;
			}
			catch (const std::exception& e) {
				saveMsg_ = std::string("読み込み失敗: ") + e.what();
			}
		}

		if (!saveMsg_.empty()) {
			ImGui::Spacing();
			bool isError = saveMsg_.find("失敗") != std::string::npos;
			ImGui::TextColored(isError ? ImVec4(1, 0.3f, 0.3f, 1) : ImVec4(0.3f, 1, 0.3f, 1), "%s", saveMsg_.c_str());
		}

		ImGui::Spacing();
		ImGui::Separator();
		if (ImGui::Button("閉じる", ImVec2(100, 0))) {
			context_.showSavePopup = false;
			saveMsg_ = "";
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
	if (!context_.showSavePopup) ImGui::CloseCurrentPopup();
#endif
}

#ifdef USE_IMGUI
void MotionEditor::DrawFileBrowser(FileBrowserState& state, const char* title)
{
	ImGui::SetNextWindowSize(ImVec2(540, 420), ImGuiCond_FirstUseEver);
	bool open = true;
	if (!ImGui::Begin(title, &open)) { ImGui::End(); if (!open) state.isOpen = false; return; }

	ImGui::TextColored(ImVec4(0.7f, 0.9f, 1, 1), "%s", state.currentDirectory.c_str());
	ImGui::Separator();

	bool canUp = !state.directoryHistory.empty();
	if (!canUp) ImGui::BeginDisabled();
	if (ImGui::Button("[ .. ] 上へ")) {
		state.currentDirectory = state.directoryHistory.back();
		state.directoryHistory.pop_back();
		state.selectedFilePath = "";
	}
	if (!canUp) ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("| ダブルクリックでフォルダを開く / ファイルを選択");
	ImGui::Separator();

	auto entries = GetDirectoryEntries(state.currentDirectory, state.filterExtension);
	ImGui::BeginChild("##fblist", ImVec2(0, -50), true);
	for (const auto& e : entries) {
		std::string name = e.path().filename().string();
		if (e.is_directory()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1, 1));
			std::string lbl = "[DIR] " + name;
			if (ImGui::Selectable(lbl.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
				if (ImGui::IsMouseDoubleClicked(0)) {
					state.directoryHistory.push_back(state.currentDirectory);
					state.currentDirectory = e.path().string();
					std::replace(state.currentDirectory.begin(), state.currentDirectory.end(), '\\', '/');
					state.selectedFilePath = "";
				}
			}
			ImGui::PopStyleColor();
		}
		else {
			bool isSel = (state.selectedFilePath == e.path().string());
			if (ImGui::Selectable(name.c_str(), isSel, ImGuiSelectableFlags_AllowDoubleClick)) {
				state.selectedFilePath = e.path().string();
				std::replace(state.selectedFilePath.begin(), state.selectedFilePath.end(), '\\', '/');
				if (ImGui::IsMouseDoubleClicked(0)) { state.isOpen = false; ImGui::EndChild(); ImGui::End(); return; }
			}
		}
	}
	ImGui::EndChild();

	ImGui::Separator();
	ImGui::TextUnformatted(state.selectedFilePath.empty() ? "(ファイルが未選択です)" : state.selectedFilePath.c_str());
	float btnX = ImGui::GetContentRegionAvail().x - 120;
	ImGui::SameLine(btnX);
	bool canOK = !state.selectedFilePath.empty();
	if (!canOK) ImGui::BeginDisabled();
	if (ImGui::Button("OK", ImVec2(55, 0))) state.isOpen = false;
	if (!canOK) ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("キャンセル", ImVec2(60, 0))) { state.selectedFilePath = ""; state.isOpen = false; }

	if (!open) state.isOpen = false;
	ImGui::End();
}
#endif

void MotionEditor::InsertKeyframeFromTransform(const std::string& bone, float time, const QuaternionTransform& tr)
{
	if (!context_.currentMotion) return;
	auto& nodeAnims = context_.currentMotion->animation_.nodeAnimations_;
	if (!nodeAnims.count(bone)) return;

	auto insertOrReplace = [&]<typename T>(auto& kfs, T val) {
		using KF = typename std::remove_reference<decltype(kfs)>::type::value_type;
		auto it = std::find_if(kfs.begin(), kfs.end(), [time](const KF& k) { return std::abs(k.time - time) < 1e-4f; });
		if (it != kfs.end()) { it->value = val; }
		else {
			KF k; k.time = time; k.value = val; kfs.push_back(k);
			std::sort(kfs.begin(), kfs.end(), [](const KF& a, const KF& b) { return a.time < b.time; });
		}
	};

	insertOrReplace(nodeAnims[bone].translate.keyframes, tr.translate);
	insertOrReplace(nodeAnims[bone].rotate.keyframes, tr.rotate);
	insertOrReplace(nodeAnims[bone].scale.keyframes, tr.scale);
}

void MotionEditor::SavePose(float time)
{
	Object3d* target = context_.GetTargetObject();
	if (!target || !target->GetModel() || !target->GetModel()->GetSkeleton() || !context_.currentMotion) return;

	Skeleton* skeleton = target->GetModel()->GetSkeleton();
	auto oldAnims = context_.currentMotion->animation_.nodeAnimations_;

	for (const auto& joint : skeleton->GetJoints()) {
		if (context_.currentMotion->animation_.nodeAnimations_.count(joint.GetName())) {
			InsertKeyframeFromTransform(joint.GetName(), time, joint.GetTransform());
		}
	}

	auto newAnims = context_.currentMotion->animation_.nodeAnimations_;

	context_.history.Execute(MakeLambdaCommand("全ポーズ保存 @ " + std::to_string(time) + "s",
		[this, newAnims]() {
			if (context_.currentMotion) context_.currentMotion->animation_.nodeAnimations_ = newAnims;
			context_.requireTimelineRebuild = true;
		},
		[this, oldAnims]() {
			if (context_.currentMotion) context_.currentMotion->animation_.nodeAnimations_ = oldAnims;
			context_.requireTimelineRebuild = true;
		}
	));
	context_.requireTimelineRebuild = true;
}

void MotionEditor::SetJointTransform(const std::string& bone, const QuaternionTransform& tr)
{
	Joint* j = FindJoint(bone);
	if (!j) return;
	j->SetTransform(tr);
	Object3d* target = context_.GetTargetObject();
	Model* m = target ? target->GetModel() : nullptr;
	if (m && m->GetSkeleton()) {
		m->GetSkeleton()->Update();
		if (m->GetSkinCluster()) m->GetSkinCluster()->UpdateMatrixPalette(m->GetSkeleton()->GetJoints());
	}
}

Joint* MotionEditor::FindJoint(const std::string& name) const
{
	Object3d* target = context_.GetTargetObject();
	if (!target || !target->GetModel() || !target->GetModel()->GetSkeleton()) return nullptr;
	return target->GetModel()->GetSkeleton()->GetJointByName(name);
}

QuaternionTransform MotionEditor::BufferToTransform() const
{
	QuaternionTransform tr;
	tr.translate = { context_.editT[0], context_.editT[1], context_.editT[2] };
	tr.rotate = EulerToQuaternion({ context_.editR[0] * (kPi / 180), context_.editR[1] * (kPi / 180), context_.editR[2] * (kPi / 180) });
	tr.scale = { context_.editS[0], context_.editS[1], context_.editS[2] };
	return tr;
}

void MotionEditor::SyncJointToBuffer(const std::string& bone)
{
	Joint* j = FindJoint(bone);
	if (!j) return;
	const auto& tr = j->GetTransform();
	context_.editT[0] = tr.translate.x; context_.editT[1] = tr.translate.y; context_.editT[2] = tr.translate.z;
	Vector3 e = QuaternionToEuler(tr.rotate);
	context_.editR[0] = e.x * (180 / kPi); context_.editR[1] = e.y * (180 / kPi); context_.editR[2] = e.z * (180 / kPi);
	context_.editS[0] = tr.scale.x; context_.editS[1] = tr.scale.y; context_.editS[2] = tr.scale.z;
}

void MotionEditor::SyncBufferToJoint()
{
	Joint* j = FindJoint(context_.selBone);
	if (j) {
		j->SetTransform(BufferToTransform());
		Object3d* target = context_.GetTargetObject();
		Model* m = target ? target->GetModel() : nullptr;
		if (m && m->GetSkeleton()) {
			m->GetSkeleton()->Update();
			if (m->GetSkinCluster()) m->GetSkinCluster()->UpdateMatrixPalette(m->GetSkeleton()->GetJoints());
		}
	}
}

std::string MotionEditor::AnimDisplayName(const std::string& key)
{
	auto pos = key.find('#');
	return (pos != std::string::npos) ? key.substr(pos + 1) : key;
}

std::vector<std::string> MotionEditor::FetchAnimationNames(const std::string& fullPath)
{
	std::vector<std::string> names;
	if (!fullPath.ends_with(".gltf")) return names;
	std::ifstream f(fullPath);
	if (!f.is_open()) return names;
	try {
		nlohmann::json gltf; f >> gltf;
		if (gltf.contains("animations"))
			for (const auto& a : gltf["animations"])
				if (a.contains("name")) names.push_back(a["name"].get<std::string>());
	}
	catch (...) {}
	return names;
}

std::vector<fs::directory_entry> MotionEditor::GetDirectoryEntries(const std::string& dir, const std::string& ext) const
{
	std::vector<fs::directory_entry> dirs, files;
	std::error_code ec;
	for (const auto& e : fs::directory_iterator(dir, ec)) {
		if (ec) break;
		if (e.is_directory()) { dirs.push_back(e); }
		else if (e.is_regular_file()) {
			std::string ex = e.path().extension().string();
			if (ext.empty()) {
				if (ex == ".gltf" || ex == ".obj" || ex == ".anim") files.push_back(e);
			}
			else {
				if (ex == ext) files.push_back(e);
			}
		}
	}
	auto byName = [](const fs::directory_entry& a, const fs::directory_entry& b) {
		return a.path().filename().string() < b.path().filename().string(); };
	std::sort(dirs.begin(), dirs.end(), byName);
	std::sort(files.begin(), files.end(), byName);
	dirs.insert(dirs.end(), files.begin(), files.end());
	return dirs;
}

Matrix4x4 MotionEditor::GetTargetWorldMatrix() const {
	if (context_.targetObjectId != -1) {
		auto* placedObj = ObjectManager::GetInstance()->GetObjectById(context_.targetObjectId);
		if (placedObj && placedObj->worldTransform) return placedObj->worldTransform->GetMatWorld();
	}
	return previewTransform_.matWorld_;
}

Matrix4x4 MotionEditor::GetJointWorldMatrix(const std::string& boneName) const
{
	Joint* j = FindJoint(boneName);
	if (j) return j->GetWorldTransform().matWorld_ * GetTargetWorldMatrix();
	return MakeIdentity4x4();
}

void MotionEditor::ApplyBoneGizmoTransform(const std::string& boneName, const Matrix4x4& newWorldMat)
{
	Joint* j = FindJoint(boneName);
	if (!j) return;

	Matrix4x4 parentWorld = MakeIdentity4x4();
	if (j->GetWorldTransform().parent_) {
		parentWorld = j->GetWorldTransform().parent_->matWorld_ * GetTargetWorldMatrix();
	}
	else {
		parentWorld = GetTargetWorldMatrix();
	}

	Matrix4x4 newLocalMat = newWorldMat * Inverse(parentWorld);

	float lT[3], lRDeg[3], lS[3];
#ifdef USE_IMGUI
	ImGuizmo::DecomposeMatrixToComponents(&newLocalMat.m[0][0], lT, lRDeg, lS);
#endif
	if (context_.selBone == boneName) {
		context_.editT[0] = lT[0]; context_.editT[1] = lT[1]; context_.editT[2] = lT[2];
		context_.editR[0] = lRDeg[0]; context_.editR[1] = lRDeg[1]; context_.editR[2] = lRDeg[2];
		context_.editS[0] = lS[0]; context_.editS[1] = lS[1]; context_.editS[2] = lS[2];
	}

	QuaternionTransform tr;
	tr.translate = { lT[0], lT[1], lT[2] };
	tr.rotate = EulerToQuaternion({ lRDeg[0] * (kPi / 180), lRDeg[1] * (kPi / 180), lRDeg[2] * (kPi / 180) });
	tr.scale = { lS[0], lS[1], lS[2] };

	j->SetTransform(tr);

	Object3d* target = context_.GetTargetObject();
	Model* m = target ? target->GetModel() : nullptr;
	if (m && m->GetSkeleton()) {
		m->GetSkeleton()->Update();
		if (m->GetSkinCluster()) m->GetSkinCluster()->UpdateMatrixPalette(m->GetSkeleton()->GetJoints());
	}
}