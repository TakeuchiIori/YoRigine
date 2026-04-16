#include "MotionEditor.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_internal.h>
#endif

#include "Systems/Camera/Camera.h"
#include "Model.h"
#include "Motion/MotionSystem.h"
#include "Skeleton/Joint.h"
#include "Skeleton/Skeleton.h"
#include <json.hpp>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <cmath>
#include <sstream>
#include <iomanip>

#include <Editor/Icon/EditorIcon.h>
#include "Object3D/ObjectManager.h" 

namespace fs = std::filesystem;

// ============================================================
//  カラーパレット
// ============================================================
namespace Col
{
#ifdef USE_IMGUI
	constexpr ImU32 PanelBg = IM_COL32(45, 45, 45, 255);
	constexpr ImU32 RowEven = IM_COL32(50, 50, 50, 220);
	constexpr ImU32 RowOdd = IM_COL32(44, 44, 44, 220);
	constexpr ImU32 RowSel = IM_COL32(55, 85, 145, 255);
	constexpr ImU32 RowHov = IM_COL32(65, 70, 80, 200);
	constexpr ImU32 RulerBg = IM_COL32(38, 38, 38, 255);
	constexpr ImU32 RulerLine = IM_COL32(90, 90, 90, 255);
	constexpr ImU32 RulerText = IM_COL32(170, 170, 170, 255);
	constexpr ImU32 Playhead = IM_COL32(255, 70, 50, 230);
	constexpr ImU32 PlayheadTp = IM_COL32(255, 70, 50, 100);
	constexpr ImU32 ChT = IM_COL32(255, 150, 50, 255);
	constexpr ImU32 ChR = IM_COL32(90, 200, 90, 255);
	constexpr ImU32 ChS = IM_COL32(80, 150, 255, 255);
	constexpr ImU32 KFSel = IM_COL32(255, 240, 50, 255);
	constexpr ImU32 LabelBg = IM_COL32(35, 35, 35, 255);
	constexpr ImU32 LabelText = IM_COL32(200, 200, 200, 255);
	constexpr ImU32 LabelSel = IM_COL32(255, 210, 80, 255);
	constexpr ImU32 StatusBg = IM_COL32(30, 30, 30, 255);
	constexpr ImU32 StatusText = IM_COL32(160, 220, 160, 255);
#endif
}

#ifdef USE_IMGUI
static ImU32 ChannelColor(KFChannel ch, bool selected)
{
	if (selected) return Col::KFSel;
	switch (ch) {
	case KFChannel::Translate: return Col::ChT;
	case KFChannel::Rotate:    return Col::ChR;
	case KFChannel::Scale:     return Col::ChS;
	}
	return Col::ChT;
}
#endif

// ============================================================
//  初期化 / 更新 / 描画
// ============================================================

void MotionEditor::Initialize(Camera* camera)
{
	camera_ = camera;
	previewTransform_.Initialize();
	binaryBrowser_.currentDirectory = "Resources/Binary";
	binaryBrowser_.filterExtension = ".anim";
	lineDrawer_ = std::make_unique<Line>();
	lineDrawer_->Initialize();
	lineDrawer_->SetCamera(camera_);
	lineDrawer_->SetColor({ 0.5f, 0.5f, 0.5f, 1.0f });
}

void MotionEditor::Update()
{
	Object3d* target = GetTargetObject();
	if (!target) return;

	Model* m = target->GetModel();
	MotionSystem* ms = m ? m->GetMotionSystem() : nullptr;

	if (ms) {
		float msTime = ms->GetAnimationTime();
		if (std::abs(msTime - scrubTime_) > 1e-4f) {
			scrubTime_ = msTime;
			dopeSheet_.SetSeekFrame(static_cast<int>(scrubTime_ * fps_));
		}
	}

	if (selBone_.empty()) {
		// ボーン非選択時は通常更新
	}
	else {
		if (m && m->GetSkeleton()) {
			m->GetSkeleton()->Update();
			if (m->GetSkinCluster())
				m->GetSkinCluster()->UpdateMatrixPalette(m->GetSkeleton()->GetJoints());
		}
	}
	previewTransform_.UpdateMatrix();
}

void MotionEditor::DrawBone()
{
	Object3d* target = GetTargetObject();
	if (isDrawBone_ && target) {
		target->DrawBone(*lineDrawer_.get(), previewTransform_.matWorld_);
	}
}

void MotionEditor::SetTargetObjectId(int id) {
	Object3d* obj = nullptr;
	if (id != -1) {
		obj = ObjectManager::GetInstance()->GetObject3dById(id);
		if (obj && obj->GetModel()) {
			if (obj->GetModel()->GetMotionSystem() == nullptr) {
				id = -1;
			}
		}
		else {
			id = -1;
		}
	}

	if (targetObjectId_ != id) {
		targetObjectId_ = id;

		if (targetObjectId_ != -1) {
			Object3d* target = GetTargetObject();
			Model* model = target->GetModel();
			MotionSystem* ms = model->GetMotionSystem();

			loadFileName_ = model->GetName();
			currentMotion_ = ms->GetAnimation();
			selectedAnimKey_ = "";

			if (currentMotion_) {
				for (auto& [key, motion] : Model::animationCache_) {
					if (&motion == currentMotion_) {
						selectedAnimKey_ = key;
						break;
					}
				}
				tracksDirty_ = true;
			}

			scrubTime_ = ms->GetAnimationTime();
			dopeSheet_.SetSeekFrame(static_cast<int>(scrubTime_ * fps_));
			isPlaying_ = true; // 選択時は基本的に再生状態とする

			statusMsg_ = "対象オブジェクトを同期: " + loadFileName_;
		}
		else {
			currentMotion_ = nullptr;
			selectedAnimKey_ = "";
			selBone_ = "";
			selKF_.Clear();
			loadFileName_ = "";
			statusMsg_ = "Ready";
		}
	}
}

// ============================================================
//  ShowEditor  ─  メインウィンドウ
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

	Object3d* target = GetTargetObject();

	// フォーカス中のキー入力
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
		history_.HandleKeyInput();

		// スペースキーで再生/一時停止のトグル
		if (ImGui::IsKeyPressed(ImGuiKey_Space) && target) {
			selBone_ = "";
			selKF_.Clear();
			if (isPlaying_) {
				if (target->GetModel() && target->GetModel()->GetMotionSystem()) {
					target->GetModel()->GetMotionSystem()->Stop();
				}
				isPlaying_ = false;
				statusMsg_ = "[ Space ] 一時停止";
			}
			else {
				if (target->GetModel() && target->GetModel()->GetMotionSystem()) {
					auto* ms = target->GetModel()->GetMotionSystem();
					float savedTime = ms->GetAnimationTime();
					// 最後まで行っていたら最初から再生
					if (savedTime >= ms->GetDuration() || ms->IsFinished()) savedTime = 0.0f;

					if (isLoop_) target->PlayLoop(); else target->PlayOnce();
					ms->SetAnimationTime(savedTime); // 時間を上書きして続きから再生
				}
				isPlaying_ = true;
				statusMsg_ = "[ Space ] 再生";
			}
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Delete) && selKF_.IsValid() && currentMotion_) {
			auto& na = currentMotion_->animation_.nodeAnimations_[selKF_.boneName];
			float t = 0;
			if (selKF_.index < (int)na.translate.keyframes.size())
				t = na.translate.keyframes[selKF_.index].time;
			DeleteKeyframe(selKF_.boneName, t);
			selKF_.Clear();
			statusMsg_ = "[ Del ] キーフレーム削除";
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

	DrawToolbar();
	ImGui::Separator();

	ImGuiIO& io = ImGui::GetIO();
	float contentW = ImGui::GetContentRegionAvail().x;
	float contentH = ImGui::GetContentRegionAvail().y - 22.0f;

	bool showTimeline = ImGui::CollapsingHeader("TIMELINE (タイムライン)", ImGuiTreeNodeFlags_DefaultOpen);

	float upperH = contentH;
	if (showTimeline) {
		if (timelineH_ > contentH - 80.0f) timelineH_ = contentH - 80.0f;
		if (timelineH_ < 50.0f) timelineH_ = 50.0f;
		upperH = contentH - timelineH_ - 12.0f;
	}

	ImGui::BeginChild("##upper", ImVec2(contentW, upperH), false);
	{
		ImGui::BeginChild("##bones", ImVec2(bonePanelW_, 0), true);
		DrawBonePanel();
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		ImGui::InvisibleButton("##vsep", ImVec2(4, upperH));
		if (ImGui::IsItemActive()) {
			bonePanelW_ = std::clamp(bonePanelW_ + io.MouseDelta.x, 80.0f, contentW - 100.0f);
		}
		if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 p = ImGui::GetItemRectMin();
			ImVec2 q = ImGui::GetItemRectMax();
			dl->AddRectFilled(p, q, IM_COL32(100, 130, 200, 180));
		}
		ImGui::PopStyleVar();

		ImGui::SameLine();

		ImGui::BeginChild("##props", ImVec2(0, 0), true);
		DrawPropertyPanel();
		ImGui::EndChild();
	}
	ImGui::EndChild();

	if (showTimeline) {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		ImGui::InvisibleButton("##hsep", ImVec2(contentW, 4.0f));
		if (ImGui::IsItemActive()) {
			timelineH_ -= io.MouseDelta.y;
		}
		if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 p = ImGui::GetItemRectMin();
			ImVec2 q = ImGui::GetItemRectMax();
			dl->AddRectFilled(p, q, IM_COL32(100, 130, 200, 180));
		}
		ImGui::PopStyleVar();

		ImGui::BeginChild("##timeline", ImVec2(contentW, timelineH_), true);
		DrawTimeline();
		ImGui::EndChild();
	}

	DrawStatusBar();

	ImGui::End();
#endif
}

// ============================================================
//  メニューバー
// ============================================================

void MotionEditor::DrawMenuBar()
{
#ifdef USE_IMGUI
	if (!ImGui::BeginMenuBar()) return;

	if (ImGui::BeginMenu("ファイル (File)")) {
		if (ImGui::MenuItem("バイナリ保存/読込...", ""))  showSavePopup_ = true;
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("編集 (Edit)")) {
		if (ImGui::MenuItem("元に戻す", "Ctrl+Z", false, history_.CanUndo())) history_.Undo();
		if (ImGui::MenuItem("やり直す", "Ctrl+Y", false, history_.CanRedo())) history_.Redo();
		ImGui::Separator();
		if (ImGui::MenuItem("履歴をクリア")) history_.Clear();
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("表示 (View)")) {
		ImGui::SliderFloat("タイムライン高さ", &timelineH_, 80.0f, 500.0f, "%.0f px");
		ImGui::SliderFloat("ボーンリスト幅", &bonePanelW_, 80.0f, 450.0f, "%.0f px");
		ImGui::SliderFloat("ズーム (px/秒)", &timelineZoom_, 20.0f, 400.0f, "%.0f");
		ImGui::EndMenu();
	}

	ImGui::Separator();
	if (!history_.CanUndo()) ImGui::BeginDisabled();
	if (ImGui::SmallButton(" << ")) { history_.Undo(); statusMsg_ = "元に戻す"; }
	if (!history_.CanUndo()) ImGui::EndDisabled();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("元に戻す (Ctrl+Z)");

	ImGui::SameLine(0, 2);
	if (!history_.CanRedo()) ImGui::BeginDisabled();
	if (ImGui::SmallButton(" >> ")) { history_.Redo(); statusMsg_ = "やり直す"; }
	if (!history_.CanRedo()) ImGui::EndDisabled();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("やり直す (Ctrl+Y)");

	ImGui::EndMenuBar();
#endif
}

// ============================================================
//  ツールバー
// ============================================================

void MotionEditor::DrawToolbar()
{
#ifdef USE_IMGUI
	Object3d* target = GetTargetObject();
	Model* model = target ? target->GetModel() : nullptr;

	if (model) {
		ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "[%s]", model->GetName().c_str());
		ImGui::SameLine(0, 12);
	}

	// ---- アニメーション選択コンボ ----
	ImGui::Text("Animation:");
	ImGui::SameLine(0, 4);
	ImGui::SetNextItemWidth(170);
	std::string selDisp = AnimDisplayName(selectedAnimKey_);
	if (selDisp.empty()) selDisp = "(未選択)";

	if (ImGui::BeginCombo("##anim", selDisp.c_str())) {
		for (auto& [key, motion] : Model::animationCache_) {
			if (!loadFileName_.empty() && key.find(loadFileName_) == std::string::npos && key.find("Binary:") == std::string::npos) {
				continue;
			}

			std::string dn = AnimDisplayName(key);
			std::string label = dn.empty() ? ("(デフォルトアニメーション)##" + key) : dn;

			bool isSel = (selectedAnimKey_ == key);
			if (ImGui::Selectable(label.c_str(), isSel)) {
				selectedAnimKey_ = key;
				currentMotion_ = &motion;
				if (target) {
					target->SetChangeMotion(loadFileName_, isLoop_ ? MotionPlayMode::Loop : MotionPlayMode::Once, dn);
				}
				isPlaying_ = true; // アニメ切り替え時は自動再生
				statusMsg_ = "アニメーション変更: " + label;
				tracksDirty_ = true;
			}
			if (isSel) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("選択中のキャラクターのアニメーションを切り替えます");

	ImGui::SameLine(0, 14);

	// ==========================================================
	// ★ 動画プレイヤー風のアイコンボタン
	// ==========================================================

	// Play/Pause (トグル)
	if (isPlaying_) {
		if (ImGui::Button((std::string(Icon::Pause) + " 一時停止").c_str())) {
			if (target && target->GetModel() && target->GetModel()->GetMotionSystem()) {
				target->GetModel()->GetMotionSystem()->Stop();
			}
			isPlaying_ = false;
			statusMsg_ = "一時停止";
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("一時停止 (Pause)");
	}
	else {
		if (ImGui::Button((std::string(Icon::Play) + " 再生").c_str())) {
			if (target && target->GetModel() && target->GetModel()->GetMotionSystem()) {
				auto* ms = target->GetModel()->GetMotionSystem();
				float savedTime = ms->GetAnimationTime();
				// もし最後まで再生されていたら最初から
				if (savedTime >= ms->GetDuration() || ms->IsFinished()) {
					savedTime = 0.0f;
				}
				if (isLoop_) target->PlayLoop(); else target->PlayOnce();
				ms->SetAnimationTime(savedTime);
			}
			isPlaying_ = true;
			statusMsg_ = "再生 (Play)";
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("再生 (Play / Resume)");
	}

	ImGui::SameLine(0, 3);

	// Stop (停止して時間をリセット)
	if (ImGui::Button((std::string(Icon::Stop) + " 停止").c_str())) {
		if (target && target->GetModel() && target->GetModel()->GetMotionSystem()) {
			target->GetModel()->GetMotionSystem()->Stop();
			target->GetModel()->GetMotionSystem()->SetAnimationTime(0.0f);
		}
		isPlaying_ = false;
		scrubTime_ = 0.0f;
		dopeSheet_.SetSeekFrame(0);
		statusMsg_ = "停止 (先頭に戻る)";
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("停止して先頭に戻る (Stop)");

	ImGui::SameLine(0, 3);

	// Loop (トグル)
	if (isLoop_) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f)); // オン時は緑色
		if (ImGui::Button((std::string(Icon::Infinity) + " ループ解除").c_str())) {
			isLoop_ = false;
			if (target && isPlaying_) {
				auto* ms = target->GetModel()->GetMotionSystem();
				float savedTime = ms->GetAnimationTime();
				target->PlayOnce();
				ms->SetAnimationTime(savedTime);
			}
			statusMsg_ = "ループ解除";
		}
		ImGui::PopStyleColor();
	}
	else {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); // オフ時はグレー
		if (ImGui::Button(" ∞ ")) {
			isLoop_ = true;
			if (target && isPlaying_) {
				auto* ms = target->GetModel()->GetMotionSystem();
				float savedTime = ms->GetAnimationTime();
				target->PlayLoop();
				ms->SetAnimationTime(savedTime);
			}
			statusMsg_ = "ループ有効";
		}
		ImGui::PopStyleColor();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("ループ再生の切り替え (Loop)");

	ImGui::SameLine(0, 12);

	// ---- スピードと長さ ----
	if (model && model->GetMotionSystem()) {
		float spd = model->GetMotionSystem()->GetCurrentAnimationSpeed();
		ImGui::Text("Speed:");
		ImGui::SameLine(0, 4);
		ImGui::SetNextItemWidth(70);
		if (ImGui::DragFloat("##spd", &spd, 0.05f, 0.0f, 5.0f, "x%.2f")) {
			if (target) target->SetMotionSpeed(spd);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("再生速度倍率  1.0 = 通常速度");
	}

	if (currentMotion_) {
		ImGui::SameLine(0, 12);
		float dur = currentMotion_->GetDuration();
		ImGui::Text("Duration:");
		ImGui::SameLine(0, 4);
		ImGui::SetNextItemWidth(70);
		if (ImGui::DragFloat("##dur", &dur, 0.01f, 0.0f, 9999.0f, "%.2fs"))
			currentMotion_->SetDuration(dur);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("アニメーションの全体の長さ（秒）");
	}

	ImGui::SameLine(0, 20);
	ImGui::Checkbox("ボーン表示", &isDrawBone_);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("メッシュ描画とボーン（線）描画を切り替えます");
	}

	ImGui::SameLine(0, 20);
	if (ImGui::Button("Save/Load..."))  showSavePopup_ = true;
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("バイナリ保存・読み込み");
#endif
}

// ============================================================
//  ボーンパネル (左)
// ============================================================

void MotionEditor::DrawBonePanel()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("BONES (アーマチュア)", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("編集するボーンを選択します\n選択後、右のパネルで値を変更できます");
	ImGui::Separator();

	Object3d* target = GetTargetObject();
	Model* model = target ? target->GetModel() : nullptr;
	if (!model || !model->GetSkeleton()) {
		ImGui::TextDisabled("スケルトンなし");
		return;
	}

	auto& joints = model->GetSkeleton()->GetJoints();

	for (const auto& joint : joints) {
		const std::string& name = joint.GetName();
		bool isSel = (selBone_ == name);

		int depth = 0;
		{
			auto par = const_cast<Joint&>(joint).GetParent();
			while (par.has_value()) {
				++depth;
				par = joints[par.value()].GetParent();
			}
		}
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + depth * 12.0f);

		if (isSel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.2f, 1.0f));

		std::string label = (depth > 0 ? "|  " : "") + name + "##b";
		if (ImGui::Selectable(label.c_str(), isSel, 0, ImVec2(0, 0))) {
			selBone_ = name;
			SyncJointToBuffer(name);
			statusMsg_ = "ボーン選択: " + name;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("クリックで選択\n選択後、右パネルの位置・回転・拡縮を編集できます");

		if (isSel) ImGui::PopStyleColor();
	}

	ImGui::Separator();
	if (ImGui::SmallButton("選択解除")) {
		selBone_ = "";
		statusMsg_ = "ボーン選択を解除しました";
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("ボーン編集モードを終了し、通常のアニメーション再生に戻ります");
#endif
}

// ============================================================
//  プロパティパネル (右)
// ============================================================

void MotionEditor::DrawPropertyPanel()
{
#ifdef USE_IMGUI
	bool boneOpen = ImGui::CollapsingHeader("BONE TRANSFORM  (位置・回転・拡縮)",
		ImGuiTreeNodeFlags_DefaultOpen);
	if (boneOpen)
	{
		if (selBone_.empty()) {
			ImGui::TextDisabled("左のリストからボーンを選択してください");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f),
				"選択中: %s", selBone_.c_str());
			ImGui::TextDisabled("ドラッグで変更 / Ctrl+クリックで直接入力");
			ImGui::Separator();

			auto handleDrag = [&](const char* label, const char* tip,
				float* buf, float spd, float vmin, float vmax)
				{
					ImGui::Text("%s", label);
					ImGui::SameLine(90);
					ImGui::SetNextItemWidth(-1);
					std::string id = std::string("##") + label;
					bool changed = ImGui::DragFloat3(id.c_str(), buf, spd, vmin, vmax);

					if (ImGui::IsItemActivated()) {
						draggingBone_ = true;
						boneSnap_ = BufferToTransform();
					}
					if (draggingBone_ && ImGui::IsItemDeactivatedAfterEdit()) {
						draggingBone_ = false;
						QuaternionTransform oldTr = boneSnap_;
						QuaternionTransform newTr = BufferToTransform();
						std::string bn = selBone_;
						history_.Execute(MakeLambdaCommand(
							"ボーン編集: " + bn,
							[this, bn, newTr]() { SetJointTransform(bn, newTr); },
							[this, bn, oldTr]() { SetJointTransform(bn, oldTr); }
						));
						statusMsg_ = "ボーン変更: " + bn;
					}
					if (changed) SyncBufferToJoint();
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
				};

			handleDrag("位置 (T)", "X / Y / Z 座標\nドラッグで移動",
				editT_, 0.01f, -FLT_MAX, FLT_MAX);
			handleDrag("回転 (R)", "X / Y / Z オイラー角 (度)\nドラッグで回転",
				editR_, 0.5f, -FLT_MAX, FLT_MAX);
			handleDrag("拡縮 (S)", "X / Y / Z スケール\nドラッグで拡縮 (1.0 = 等倍)",
				editS_, 0.01f, 0.001f, 100.0f);

			ImGui::Spacing();

			if (currentMotion_) {
				ImGui::Separator();
				ImGui::Text("現在時刻:");
				ImGui::SameLine(90);
				ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%.4f 秒", scrubTime_);

				if (ImGui::Button("[ KF ] キーフレームを打つ", ImVec2(-1, 0))) {
					InsertKeyframe(selBone_, scrubTime_);
					statusMsg_ = "KF 挿入: " + selBone_ + " @ " + std::to_string(scrubTime_) + "s";
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("現在の姿勢をこの時刻にキーフレームとして記録します\n"
						"下のタイムラインに◆が追加されます");
			}
			else {
				ImGui::TextDisabled("(アニメーションを選択するとKFを打てます)");
			}
		}
	}

	ImGui::Spacing();

	bool kfOpen = ImGui::CollapsingHeader("KEYFRAME  (選択中のキーフレーム値)",
		ImGuiTreeNodeFlags_DefaultOpen);
	if (kfOpen)
	{
		if (!selKF_.IsValid()) {
			ImGui::TextDisabled("タイムラインで◆をクリックしてキーフレームを選択してください");
		}
		else if (!currentMotion_) {
			ImGui::TextDisabled("アニメーションがありません");
		}
		else {
			auto& nodeAnims = currentMotion_->animation_.nodeAnimations_;
			auto it = nodeAnims.find(selKF_.boneName);
			if (it == nodeAnims.end()) { ImGui::TextDisabled("データなし"); }
			else {
				auto& na = it->second;
				int   idx = selKF_.index;
				std::string bone = selKF_.boneName;

				ImGui::Text("ボーン: %s", bone.c_str());

				float kfTime = 0;
				if (idx < (int)na.translate.keyframes.size())
					kfTime = na.translate.keyframes[idx].time;

				ImGui::Text("時刻:");
				ImGui::SameLine(90);
				ImGui::SetNextItemWidth(120);
				float newT = kfTime;
				if (ImGui::DragFloat("##kftime", &newT, 0.001f, 0.0f,
					currentMotion_->GetDuration(), "%.4f s")) {
					MoveKeyframe(bone, KFChannel::Translate, idx, newT);
					MoveKeyframe(bone, KFChannel::Rotate, idx, newT);
					MoveKeyframe(bone, KFChannel::Scale, idx, newT);
					scrubTime_ = newT;
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("このキーフレームの時刻を変更します\nタイムライン上でドラッグしても移動できます");

				ImGui::Separator();

				if (ImGui::BeginTabBar("##kftab")) {

					bool tOpen = ImGui::BeginTabItem("位置 (T)");
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("キーフレームの位置値を編集します");
					if (tOpen) {
						if (idx < (int)na.translate.keyframes.size()) {
							auto& kf = na.translate.keyframes[idx];
							float v[3] = { kf.value.x, kf.value.y, kf.value.z };
							if (ImGui::IsItemActivated()) kfSnapT_ = kf.value;
							ImGui::SetNextItemWidth(-1);
							bool ch = ImGui::DragFloat3("##kft", v, 0.001f);
							if (ImGui::IsItemActivated()) kfSnapT_ = kf.value;
							if (ImGui::IsItemDeactivatedAfterEdit()) {
								Vector3 ov = kfSnapT_, nv = { v[0],v[1],v[2] };
								history_.Execute(MakeLambdaCommand("KF位置編集: " + bone,
									[this, bone, idx, nv]() { currentMotion_->animation_.nodeAnimations_[bone].translate.keyframes[idx].value = nv; },
									[this, bone, idx, ov]() { currentMotion_->animation_.nodeAnimations_[bone].translate.keyframes[idx].value = ov; }));
								statusMsg_ = "KF位置変更: " + bone;
							}
							else if (ch) { kf.value = { v[0],v[1],v[2] }; }
						}
						else { ImGui::TextDisabled("この KF に位置データなし"); }
						ImGui::EndTabItem();
					}

					bool rOpen = ImGui::BeginTabItem("回転 (R)");
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("キーフレームの回転値を編集します\n内部はクォータニオンですが度数で表示します");
					if (rOpen) {
						if (idx < (int)na.rotate.keyframes.size()) {
							auto& kf = na.rotate.keyframes[idx];
							Vector3 e = QuaternionToEuler(kf.value);
							float deg[3] = { e.x * (180 / kPi), e.y * (180 / kPi), e.z * (180 / kPi) };
							if (ImGui::IsItemActivated()) kfSnapR_ = kf.value;
							ImGui::SetNextItemWidth(-1);
							bool ch = ImGui::DragFloat3("##kfr", deg, 0.1f);
							if (ImGui::IsItemActivated()) kfSnapR_ = kf.value;
							if (ImGui::IsItemDeactivatedAfterEdit()) {
								Quaternion ov = kfSnapR_;
								Quaternion nv = EulerToQuaternion({ deg[0] * (kPi / 180), deg[1] * (kPi / 180), deg[2] * (kPi / 180) });
								history_.Execute(MakeLambdaCommand("KF回転編集: " + bone,
									[this, bone, idx, nv]() { currentMotion_->animation_.nodeAnimations_[bone].rotate.keyframes[idx].value = nv; },
									[this, bone, idx, ov]() { currentMotion_->animation_.nodeAnimations_[bone].rotate.keyframes[idx].value = ov; }));
								statusMsg_ = "KF回転変更: " + bone;
							}
							else if (ch) {
								kf.value = EulerToQuaternion({ deg[0] * (kPi / 180), deg[1] * (kPi / 180), deg[2] * (kPi / 180) });
							}
						}
						else { ImGui::TextDisabled("この KF に回転データなし"); }
						ImGui::EndTabItem();
					}

					bool sOpen = ImGui::BeginTabItem("拡縮 (S)");
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("キーフレームのスケール値を編集します");
					if (sOpen) {
						if (idx < (int)na.scale.keyframes.size()) {
							auto& kf = na.scale.keyframes[idx];
							float v[3] = { kf.value.x, kf.value.y, kf.value.z };
							if (ImGui::IsItemActivated()) kfSnapS_ = kf.value;
							ImGui::SetNextItemWidth(-1);
							bool ch = ImGui::DragFloat3("##kfs", v, 0.001f, 0.001f, 100.0f);
							if (ImGui::IsItemActivated()) kfSnapS_ = kf.value;
							if (ImGui::IsItemDeactivatedAfterEdit()) {
								Vector3 ov = kfSnapS_, nv = { v[0],v[1],v[2] };
								history_.Execute(MakeLambdaCommand("KF拡縮編集: " + bone,
									[this, bone, idx, nv]() { currentMotion_->animation_.nodeAnimations_[bone].scale.keyframes[idx].value = nv; },
									[this, bone, idx, ov]() { currentMotion_->animation_.nodeAnimations_[bone].scale.keyframes[idx].value = ov; }));
								statusMsg_ = "KFスケール変更: " + bone;
							}
							else if (ch) { kf.value = { v[0],v[1],v[2] }; }
						}
						else { ImGui::TextDisabled("この KF に拡縮データなし"); }
						ImGui::EndTabItem();
					}

					ImGui::EndTabBar();
				}

				ImGui::Spacing();
				ImGui::Separator();

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.12f, 0.12f, 1));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.20f, 0.20f, 1));
				if (ImGui::Button("このキーフレームを削除  [Delete]", ImVec2(-1, 0))) {
					DeleteKeyframe(bone, kfTime);
					selKF_.Clear();
					statusMsg_ = "KF 削除: " + bone;
				}
				ImGui::PopStyleColor(2);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("選択中のキーフレームを削除します\nCtrl+Z で元に戻せます");
			}
		}
	}

	ImGui::Spacing();

	if (ImGui::CollapsingHeader("HISTORY  (操作履歴)")) {
		history_.DrawImGui();
	}
#endif
}

// ============================================================
//  タイムライン
// ============================================================

void MotionEditor::DrawTimeline()
{
#ifdef USE_IMGUI
	if (!currentMotion_) {
		ImGui::TextDisabled("アニメーションを選択するとタイムラインが表示されます");
		return;
	}

	auto& nodeAnims = currentMotion_->animation_.nodeAnimations_;
	if (nodeAnims.empty()) {
		ImGui::TextDisabled("アニメーションデータなし");
		return;
	}

	float duration = currentMotion_->GetDuration();
	const int totalFrames = std::max(1, static_cast<int>(duration * fps_));

	float canvasW = ImGui::GetContentRegionAvail().x;

	ImGui::Text("Time:");
	ImGui::SameLine(0, 4);
	ImGui::SetNextItemWidth(canvasW - 300);

	Object3d* target = GetTargetObject();

	// スライダー操作時に一時停止＆同期
	if (ImGui::SliderFloat("##scrub", &scrubTime_, 0.0f, duration, "%.3f s")) {
		if (target && target->GetModel() && target->GetModel()->GetMotionSystem()) {
			auto* ms = target->GetModel()->GetMotionSystem();
			ms->Stop();
			isPlaying_ = false; // ★ 一時停止状態にする
			ms->SetAnimationTime(scrubTime_);
		}
		dopeSheet_.SetSeekFrame(static_cast<int>(scrubTime_ * fps_));
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("クリック/ドラッグで時刻を移動");

	ImGui::SameLine(0, 8);
	if (ImGui::SmallButton("|<")) {
		scrubTime_ = 0;
		if (target && target->GetModel() && target->GetModel()->GetMotionSystem()) {
			target->GetModel()->GetMotionSystem()->SetAnimationTime(0.0f);
		}
		dopeSheet_.SetSeekFrame(0);
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("先頭に戻る");

	ImGui::SameLine(0, 14);
	ImGui::TextColored(ImVec4(1.0f, 0.59f, 0.2f, 1.0f), (std::string(Icon::ArrowsAlt) + " 位置").c_str());
	ImGui::SameLine(0, 6);
	ImGui::TextColored(ImVec4(0.35f, 0.78f, 0.35f, 1.0f), (std::string(Icon::SyncAlt) + " 回転").c_str());
	ImGui::SameLine(0, 6);
	ImGui::TextColored(ImVec4(0.31f, 0.59f, 1.0f, 1.0f), (std::string(Icon::ExpandArrowsAlt) + " 拡縮").c_str());
	ImGui::SameLine(0, 6);
	ImGui::TextColored(ImVec4(1.0f, 0.94f, 0.2f, 1.0f), (std::string(Icon::CheckCircle) + " 選択中").c_str());

	ImGui::Separator();

	if (tracksDirty_) {
		RebuildTracks();
		tracksDirty_ = false;
	}

	// ドープシート操作時に一時停止＆同期
	dopeSheet_.SetSeekCallback([this](int frame) {
		scrubTime_ = frame / static_cast<float>(fps_);
		Object3d* t = GetTargetObject();
		if (t && t->GetModel()) {
			auto* ms = t->GetModel()->GetMotionSystem();
			if (ms) {
				ms->Stop();
				isPlaying_ = false; // ★ 一時停止状態にする
				ms->SetAnimationTime(scrubTime_);
			}
		}
		});

	dopeSheet_.SetDeleteKeyCallback([this](int trackIdx, int keyIdx) {
		if (trackIdx >= static_cast<int>(tracks_.size())) return;
		if (!currentMotion_) return;
		if (keyIdx >= static_cast<int>(tracks_[trackIdx].keys.size())) return;
		int frame = tracks_[trackIdx].keys[keyIdx].frame;
		float t = frame / static_cast<float>(fps_);
		DeleteKeyframe(selKF_.boneName, t);
		tracksDirty_ = true;
		statusMsg_ = "KF 削除";
		});

	bool changed = dopeSheet_.Draw("MotionTimeline", tracks_, totalFrames, fps_);

	if (changed) {
		ApplyTracksToMotion();
		tracksDirty_ = true;
		statusMsg_ = "KF 移動完了";
	}

	{
		int seekFrame = dopeSheet_.GetSeekFrame();
		float newTime = seekFrame / static_cast<float>(fps_);
		if (std::abs(newTime - scrubTime_) > 1e-4f) {
			scrubTime_ = newTime;
		}
	}

	if (draggingKF_ && !ImGui::IsMouseDown(0)) {
		draggingKF_ = false;
		statusMsg_ = "KF 移動完了";
	}
#endif
}

// ============================================================
//  RebuildTracks  ─ Motion → DopeTrack[] 変換
// ============================================================

void MotionEditor::RebuildTracks()
{
	tracks_.clear();
	trackBoneMap_.clear();
	if (!currentMotion_) return;

	static const DopeSheet::Color colT = { 1.0f, 0.59f, 0.2f, 1.0f };
	static const DopeSheet::Color colR = { 0.35f, 0.78f, 0.35f, 1.0f };
	static const DopeSheet::Color colS = { 0.31f, 0.59f, 1.0f, 1.0f };

	auto& nodeAnims = currentMotion_->animation_.nodeAnimations_;

	std::vector<std::string> boneNames;
	boneNames.reserve(nodeAnims.size());
	for (const auto& [name, _] : nodeAnims) boneNames.push_back(name);
	std::sort(boneNames.begin(), boneNames.end());

	for (const auto& boneName : boneNames)
	{
		const auto& na = nodeAnims.at(boneName);

		{
			DopeSheet::DopeTrack header;
			header.label = boneName;
			header.isGroupHeader = true;
			header.groupExpanded = (selBone_ == boneName || selKF_.boneName == boneName
				? true : header.groupExpanded);
			tracks_.push_back(header);
			trackBoneMap_.push_back({ boneName, -1 });
		}

		{
			DopeSheet::DopeTrack t;
			t.label = "  T";
			t.color = colT;
			t.groupDepth = 1;
			for (const auto& kf : na.translate.keyframes)
				t.keys.emplace_back(static_cast<int>(kf.time * fps_ + 0.5f), kf.value.x, 0);
			tracks_.push_back(t);
			trackBoneMap_.push_back({ boneName, 0 });
		}
		{
			DopeSheet::DopeTrack r;
			r.label = "  R";
			r.color = colR;
			r.groupDepth = 1;
			for (const auto& kf : na.rotate.keyframes)
				r.keys.emplace_back(static_cast<int>(kf.time * fps_ + 0.5f), kf.value.w, 1);
			tracks_.push_back(r);
			trackBoneMap_.push_back({ boneName, 1 });
		}
		{
			DopeSheet::DopeTrack s;
			s.label = "  S";
			s.color = colS;
			s.groupDepth = 1;
			for (const auto& kf : na.scale.keyframes)
				s.keys.emplace_back(static_cast<int>(kf.time * fps_ + 0.5f), kf.value.x, 2);
			tracks_.push_back(s);
			trackBoneMap_.push_back({ boneName, 2 });
		}
	}
}

// ============================================================
//  ApplyTracksToMotion  ─ DopeTrack[] → Motion KF時刻書き戻し
// ============================================================

void MotionEditor::ApplyTracksToMotion()
{
	if (!currentMotion_) return;

	for (int ti = 0; ti < static_cast<int>(tracks_.size()); ++ti)
	{
		if (ti >= static_cast<int>(trackBoneMap_.size())) break;
		const auto& [boneName, channel] = trackBoneMap_[ti];
		if (channel < 0) continue;
		if (!currentMotion_->animation_.nodeAnimations_.count(boneName)) continue;

		auto& na = currentMotion_->animation_.nodeAnimations_[boneName];
		const auto& keys = tracks_[ti].keys;

		auto applyTimes = [&](auto& keyframes) {
			if (keys.size() != keyframes.size()) return;
			for (int ki = 0; ki < static_cast<int>(keys.size()); ++ki)
				keyframes[ki].time = keys[ki].frame / static_cast<float>(fps_);
			std::sort(keyframes.begin(), keyframes.end(),
				[](const auto& a, const auto& b) { return a.time < b.time; });
			};

		switch (channel) {
		case 0: applyTimes(na.translate.keyframes); break;
		case 1: applyTimes(na.rotate.keyframes);    break;
		case 2: applyTimes(na.scale.keyframes);     break;
		}
	}
}

#ifdef USE_IMGUI
void MotionEditor::DrawKFRow(ImDrawList* /*dl*/,
	const std::string& /*boneName*/, KFChannel /*ch*/,
	float /*rowY*/, float /*canvasX*/, float /*canvasW*/, float /*labelW*/)
{
}
#endif

// ============================================================
//  ステータスバー
// ============================================================

void MotionEditor::DrawStatusBar()
{
#ifdef USE_IMGUI
	float barH = 20.0f;
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
	ImGui::BeginChild("##status", ImVec2(0, barH), false);

	ImGui::SetCursorPosY(3);
	ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "  %s", statusMsg_.c_str());

	float hints_x = ImGui::GetContentRegionAvail().x - 400;
	if (hints_x > 200) {
		ImGui::SameLine(hints_x);
		ImGui::TextDisabled("Space=再生  Del=KF削除  Ctrl+Z=元に戻す  Ctrl+Y=やり直す  ホイール=スクロール  Ctrl+ホイール=ズーム");
	}

	ImGui::EndChild();
	ImGui::PopStyleColor();
#endif
}

// ============================================================
//  バイナリ保存・読み込みポップアップ
// ============================================================

void MotionEditor::DrawSaveLoadPopup()
{
#ifdef USE_IMGUI
	if (showSavePopup_)
		ImGui::OpenPopup("保存 / 読み込み##popup");

	ImGui::SetNextWindowSize(ImVec2(500, 280), ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal("保存 / 読み込み##popup", &showSavePopup_)) {

		ImGui::Text("バイナリファイル (.anim):");
		{
			char buf[512]; strncpy_s(buf, sizeof(buf), savePath_.c_str(), _TRUNCATE);
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90);
			if (ImGui::InputText("##savepath", buf, sizeof(buf))) savePath_ = buf;
			ImGui::SameLine();
			if (ImGui::Button("参照##b")) {
				binaryBrowser_.isOpen = true;
				binaryBrowser_.selectedFilePath = "";
				showSavePopup_ = false;
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		bool canSave = currentMotion_ != nullptr;
		if (!canSave) ImGui::BeginDisabled();
		if (ImGui::Button("バイナリ保存", ImVec2(130, 0))) {
			try {
				currentMotion_->SaveBinary(*currentMotion_, AnimDisplayName(selectedAnimKey_), savePath_);
				saveMsg_ = "保存成功: " + savePath_;
				statusMsg_ = saveMsg_;
			}
			catch (const std::exception& e) {
				saveMsg_ = std::string("保存失敗: ") + e.what();
			}
		}
		if (!canSave) ImGui::EndDisabled();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("現在のアニメーションデータをバイナリファイルに保存します");

		ImGui::SameLine(0, 10);

		Object3d* target = GetTargetObject();

		if (ImGui::Button("バイナリ読み込み", ImVec2(140, 0))) {
			try {
				Motion loaded = Motion().LoadBinary(savePath_);
				const std::string key = "Binary:" + savePath_;
				Model::animationCache_[key] = std::move(loaded);
				selectedAnimKey_ = key;
				currentMotion_ = &Model::animationCache_[key];
				if (target) {
					target->SetChangeMotion(loadFileName_, MotionPlayMode::Loop, AnimDisplayName(key));
				}
				saveMsg_ = "読み込み成功: " + savePath_;
				statusMsg_ = saveMsg_;
			}
			catch (const std::exception& e) {
				saveMsg_ = std::string("読み込み失敗: ") + e.what();
			}
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("バイナリファイルからアニメーションを読み込みます");

		if (!saveMsg_.empty()) {
			ImGui::Spacing();
			bool isError = saveMsg_.find("失敗") != std::string::npos;
			ImGui::TextColored(isError ? ImVec4(1, 0.3f, 0.3f, 1) : ImVec4(0.3f, 1, 0.3f, 1),
				"%s", saveMsg_.c_str());
		}

		ImGui::Spacing();
		ImGui::Separator();
		if (ImGui::Button("閉じる", ImVec2(100, 0))) {
			showSavePopup_ = false;
			saveMsg_ = "";
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
	if (!showSavePopup_) ImGui::CloseCurrentPopup();
#endif
}

// ============================================================
//  ファイルブラウザ
// ============================================================

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

// ============================================================
//  キーフレーム操作
// ============================================================

void MotionEditor::InsertKeyframe(const std::string& bone, float time)
{
	if (!currentMotion_) return;
	auto& nodeAnims = currentMotion_->animation_.nodeAnimations_;
	if (!nodeAnims.count(bone)) return;

	Vector3    newT = { editT_[0], editT_[1], editT_[2] };
	Quaternion newR = EulerToQuaternion({ editR_[0] * (kPi / 180), editR_[1] * (kPi / 180), editR_[2] * (kPi / 180) });
	Vector3    newS = { editS_[0], editS_[1], editS_[2] };
	Motion::NodeAnimation snap = nodeAnims[bone];

	history_.Execute(MakeLambdaCommand("KF挿入: " + bone,
		[this, bone, time, newT, newR, newS]() {
			auto& na = currentMotion_->animation_.nodeAnimations_[bone];
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
		},
		[this, bone, snap]() {
			currentMotion_->animation_.nodeAnimations_[bone] = snap;
		}
	));
}

void MotionEditor::DeleteKeyframe(const std::string& bone, float time)
{
	if (!currentMotion_) return;
	auto& nodeAnims = currentMotion_->animation_.nodeAnimations_;
	if (!nodeAnims.count(bone)) return;

	Motion::NodeAnimation snap = nodeAnims[bone];
	history_.Execute(MakeLambdaCommand("KF削除: " + bone,
		[this, bone, time]() {
			auto& na = currentMotion_->animation_.nodeAnimations_[bone];
			auto rem = [time](auto& kfs) {
				kfs.erase(std::remove_if(kfs.begin(), kfs.end(),
					[time](const auto& k) { return std::abs(k.time - time) < 1e-4f; }), kfs.end());
				};
			rem(na.translate.keyframes);
			rem(na.rotate.keyframes);
			rem(na.scale.keyframes);
		},
		[this, bone, snap]() {
			currentMotion_->animation_.nodeAnimations_[bone] = snap;
		}
	));
}

void MotionEditor::MoveKeyframe(const std::string& bone, KFChannel ch, int idx, float newTime)
{
	if (!currentMotion_) return;
	auto& na = currentMotion_->animation_.nodeAnimations_[bone];

	auto setTime = [&](auto& kfs) {
		if (idx >= 0 && idx < (int)kfs.size()) {
			kfs[idx].time = newTime;
			std::sort(kfs.begin(), kfs.end(),
				[](const auto& a, const auto& b) { return a.time < b.time; });
		}
		};
	switch (ch) {
	case KFChannel::Translate: setTime(na.translate.keyframes); break;
	case KFChannel::Rotate:    setTime(na.rotate.keyframes);    break;
	case KFChannel::Scale:     setTime(na.scale.keyframes);     break;
	}
}

// ============================================================
//  ボーン操作
// ============================================================

void MotionEditor::SetJointTransform(const std::string& bone, const QuaternionTransform& tr)
{
	Joint* j = FindJoint(bone);
	if (!j) return;
	j->SetTransform(tr);
	Object3d* target = GetTargetObject();
	Model* m = target ? target->GetModel() : nullptr;
	if (m && m->GetSkeleton()) {
		m->GetSkeleton()->Update();
		if (m->GetSkinCluster())
			m->GetSkinCluster()->UpdateMatrixPalette(m->GetSkeleton()->GetJoints());
	}
}

Joint* MotionEditor::FindJoint(const std::string& name) const
{
	Object3d* target = GetTargetObject();
	if (!target || !target->GetModel()) return nullptr;
	return target->GetModel()->GetJointMap(name);
}

QuaternionTransform MotionEditor::BufferToTransform() const
{
	QuaternionTransform tr;
	tr.translate = { editT_[0], editT_[1], editT_[2] };
	tr.rotate = EulerToQuaternion({ editR_[0] * (kPi / 180), editR_[1] * (kPi / 180), editR_[2] * (kPi / 180) });
	tr.scale = { editS_[0], editS_[1], editS_[2] };
	return tr;
}

void MotionEditor::SyncJointToBuffer(const std::string& bone)
{
	Joint* j = FindJoint(bone);
	if (!j) return;
	const auto& tr = j->GetTransform();
	editT_[0] = tr.translate.x; editT_[1] = tr.translate.y; editT_[2] = tr.translate.z;
	Vector3 e = QuaternionToEuler(tr.rotate);
	editR_[0] = e.x * (180 / kPi); editR_[1] = e.y * (180 / kPi); editR_[2] = e.z * (180 / kPi);
	editS_[0] = tr.scale.x; editS_[1] = tr.scale.y; editS_[2] = tr.scale.z;
}

void MotionEditor::SyncBufferToJoint()
{
	Joint* j = FindJoint(selBone_);
	if (j) j->SetTransform(BufferToTransform());
}

// ============================================================
//  ユーティリティ
// ============================================================

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

std::vector<fs::directory_entry> MotionEditor::GetDirectoryEntries(
	const std::string& dir, const std::string& ext) const
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