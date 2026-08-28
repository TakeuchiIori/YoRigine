#include "MotionPathEditor.h"

#ifdef USE_IMGUI

#include "AttackMotionFactory.h"
#include "Editor/Editor.h"
#include "OrbitMotion.h"
#include "SplinePointGizmable.h"
#include "Systems/GameTime/GameTime.h"

#include <algorithm>
#include <cstdio>
#include <numbers>

void MotionPathEditor::Initialize(YoRigine::Camera *camera) {
	camera_ = camera;
	preview_.Initialize(camera);
	gizmo_.Initialize();

	MotionPathLibrary::GetInstance().Load();
	initialized_ = true;
}

MotionPathEntry *MotionPathEditor::GetSelectedEntry() {
	auto &entries = MotionPathLibrary::GetInstance().GetAll();
	if (selectedPath_ < 0 || selectedPath_ >= static_cast<int>(entries.size())) return nullptr;
	return &entries[selectedPath_];
}

// ============================================================
// プレビュー用の基準情報
//
// 経路は「攻撃を始めた瞬間の敵」を基準にしたローカル座標なので、
// 確認するには仮の位置と向きが要る。
// ============================================================
MotionContext MotionPathEditor::MakePreviewContext() const {
	MotionContext ctx;
	ctx.startPosition = previewOrigin_;
	ctx.startYaw = previewYawDeg_ * std::numbers::pi_v<float> / 180.0f;
	ctx.targetPosition = previewTarget_;
	ctx.hasTarget = true;
	ctx.scale = previewScale_;
	return ctx;
}

// ============================================================
// パネル本体
// ============================================================
void MotionPathEditor::Draw() {
	if (!initialized_) {
		ImGui::TextDisabled("初期化されていません");
		return;
	}

	// 再生位置を進める
	if (playing_) {
		playTime_ += YoRigine::GameTime::GetUnscaledDeltaTime();
		if (playTime_ > playDuration_) playTime_ = 0.0f;
	}

	DrawPathList();

	MotionPathEntry *entry = GetSelectedEntry();
	if (!entry || !entry->motion) {
		ImGui::TextDisabled("経路を選択してください");
		return;
	}

	ImGui::Separator();
	DrawPathSettings(*entry);

	ImGui::Separator();
	DrawPreviewSettings();
}

// ============================================================
// 経路一覧
// ============================================================
void MotionPathEditor::DrawPathList() {
	auto &library = MotionPathLibrary::GetInstance();
	auto &entries = library.GetAll();

	ImGui::SeparatorText("経路一覧");

	if (ImGui::BeginChild("pathList", ImVec2(0, 120), ImGuiChildFlags_Borders)) {
		for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
			const bool selected = (i == selectedPath_);
			const char *typeName = entries[i].motion ? entries[i].motion->GetTypeName() : "?";

			char label[192];
			std::snprintf(label, sizeof(label), "%s  [%s]##path%d", entries[i].name.c_str(),
			              typeName, i);

			if (ImGui::Selectable(label, selected)) {
				selectedPath_ = i;
				selectedPoint_ = -1;
			}
		}
	}
	ImGui::EndChild();

	// 追加ボタンは種類ごとに用意する
	for (const auto &typeName : AttackMotionFactory::GetTypeNames()) {
		char buttonLabel[64];
		std::snprintf(buttonLabel, sizeof(buttonLabel), "%s を追加", typeName.c_str());
		if (ImGui::Button(buttonLabel)) {
			selectedPath_ = library.Add(typeName, typeName);
			selectedPoint_ = -1;
		}
		ImGui::SameLine();
	}
	ImGui::NewLine();

	if (ImGui::Button("複製") && selectedPath_ >= 0) {
		selectedPath_ = library.Duplicate(selectedPath_);
	}
	ImGui::SameLine();
	if (ImGui::Button("削除") && selectedPath_ >= 0) {
		library.Remove(selectedPath_);
		selectedPath_ = -1;
		selectedPoint_ = -1;
	}
	ImGui::SameLine();
	if (ImGui::Button("JSONに保存")) {
		library.Save();
	}
	ImGui::SameLine();
	if (ImGui::Button("再読み込み")) {
		library.Load();
		selectedPath_ = -1;
		selectedPoint_ = -1;
	}
}

// ============================================================
// 選択中の経路の設定
// ============================================================
void MotionPathEditor::DrawPathSettings(MotionPathEntry &entry) {
	ImGui::SeparatorText("経路の設定");

	std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", entry.name.c_str());
	if (ImGui::InputText("名前", nameBuffer_, sizeof(nameBuffer_))) {
		entry.name = nameBuffer_;
	}

	if (auto *spline = dynamic_cast<SplineMotion *>(entry.motion.get())) {
		DrawSplineEditor(*spline);
	} else if (auto *orbit = dynamic_cast<OrbitMotion *>(entry.motion.get())) {
		DrawOrbitEditor(*orbit);
	}
}

// ============================================================
// 座標系の選択（スプラインと円運動で共通）
// ============================================================
namespace {
void DrawSpaceCombo(MotionSpace &space) {
	const char *items[] = {"自分基準 (selfLocal)", "相手基準 (targetRelative)", "ワールド (world)"};
	int current = static_cast<int>(space);
	if (ImGui::Combo("基準", &current, items, IM_ARRAYSIZE(items))) {
		space = static_cast<MotionSpace>(current);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip()) {
		ImGui::TextUnformatted("自分基準  : 攻撃開始時の自分の位置と向きが原点。踏み込みや後退向き\n"
		                       "相手基準  : 相手の現在位置が原点。相手が動くと経路も動く＝追尾\n"
		                       "ワールド  : 絶対座標");
		ImGui::EndTooltip();
	}
}
} // namespace

// ============================================================
// スプラインの編集
// ============================================================
void MotionPathEditor::DrawSplineEditor(SplineMotion &spline) {
	DrawSpaceCombo(spline.space);

	ImGui::Checkbox("等速で進む", &spline.constantSpeed);
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip()) {
		ImGui::TextUnformatted("OFFにすると制御点が密なところで遅くなる。\n"
		                       "ONにすると弧長で正規化して見た目の速度が一定になる。");
		ImGui::EndTooltip();
	}

	ImGui::SeparatorText("制御点");
	if (!spline.IsValid()) {
		ImGui::TextColored({1.0f, 0.5f, 0.4f, 1.0f}, "制御点が2つ以上必要です");
	}

	for (int i = 0; i < static_cast<int>(spline.points.size()); ++i) {
		ImGui::PushID(i);

		const bool selected = (i == selectedPoint_);
		if (ImGui::RadioButton("##sel", selected)) {
			selectedPoint_ = selected ? -1 : i;
		}
		ImGui::SameLine();

		ImGui::SetNextItemWidth(220.0f);
		ImGui::DragFloat3("##pos", &spline.points[i].x, 0.1f);

		ImGui::SameLine();
		if (ImGui::SmallButton("+")) {
			// 次の点との中間に挿入する
			const Vector3 next = (i + 1 < static_cast<int>(spline.points.size()))
			                         ? spline.points[i + 1]
			                         : spline.points[i] + Vector3{0.0f, 0.0f, 2.0f};
			spline.points.insert(spline.points.begin() + i + 1, (spline.points[i] + next) * 0.5f);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("x") && spline.points.size() > 1) {
			spline.points.erase(spline.points.begin() + i);
			if (selectedPoint_ >= static_cast<int>(spline.points.size())) selectedPoint_ = -1;
			ImGui::PopID();
			break;
		}

		ImGui::PopID();
	}

	if (ImGui::Button("末尾に制御点を追加")) {
		const Vector3 last =
			spline.points.empty() ? Vector3{} : spline.points.back() + Vector3{0.0f, 0.0f, 2.0f};
		spline.points.push_back(last);
	}

	ImGui::TextDisabled("ラジオボタンで選ぶとゲームビューでギズモ操作できます");
}

// ============================================================
// 円運動の編集
// ============================================================
void MotionPathEditor::DrawOrbitEditor(OrbitMotion &orbit) {
	DrawSpaceCombo(orbit.space);

	ImGui::DragFloat("開始半径", &orbit.startRadius, 0.1f, 0.0f, 50.0f, "%.2fm");
	ImGui::DragFloat("終了半径", &orbit.endRadius, 0.1f, 0.0f, 50.0f, "%.2fm");
	ImGui::DragFloat("開始角度", &orbit.startAngleDeg, 1.0f, -360.0f, 360.0f, "%.0f度");
	ImGui::DragFloat("回る角度", &orbit.sweepDeg, 1.0f, -1080.0f, 1080.0f, "%.0f度");
	ImGui::DragFloat("開始高さ", &orbit.startHeight, 0.1f, -10.0f, 20.0f, "%.2fm");
	ImGui::DragFloat("終了高さ", &orbit.endHeight, 0.1f, -10.0f, 20.0f, "%.2fm");
	ImGui::TextDisabled("半径を縮めていくと回り込みながら間合いを詰める動きになります");
}

// ============================================================
// プレビュー設定
// ============================================================
void MotionPathEditor::DrawPreviewSettings() {
	ImGui::SeparatorText("プレビュー");

	ImGui::DragFloat3("自分の位置", &previewOrigin_.x, 0.1f);
	ImGui::DragFloat("自分の向き", &previewYawDeg_, 1.0f, -180.0f, 180.0f, "%.0f度");
	ImGui::DragFloat3("相手の位置", &previewTarget_.x, 0.1f);
	ImGui::DragFloat("経路の拡大率", &previewScale_, 0.05f, 0.1f, 5.0f, "x%.2f");

	ImGui::Checkbox("再生", &playing_);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat("所要時間", &playDuration_, 0.05f, 0.1f, 10.0f, "%.2f秒");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(180.0f);
	float t = (playDuration_ > 0.0f) ? (playTime_ / playDuration_) : 0.0f;
	if (ImGui::SliderFloat("進行度", &t, 0.0f, 1.0f, "%.2f")) {
		playTime_ = t * playDuration_;
		playing_ = false;
	}
}

// ============================================================
// 3Dプレビュー描画
// ============================================================
void MotionPathEditor::DrawPreview() {
	MotionPathEntry *entry = GetSelectedEntry();
	if (!entry || !entry->motion) return;

	const float markerT = (playDuration_ > 0.0f) ? (playTime_ / playDuration_) : 0.0f;
	preview_.Draw(*entry->motion, MakePreviewContext(), selectedPoint_, markerT);
}

// ============================================================
// ギズモ描画
//
// ImGuizmo はゲームビューの ImGui コンテキスト内で呼ぶ必要があるので、
// Editor のギズモコールバックから呼ばれる。
// ============================================================
void MotionPathEditor::DrawGizmo() {
	if (!camera_ || selectedPoint_ < 0) return;

	MotionPathEntry *entry = GetSelectedEntry();
	if (!entry) return;

	auto *spline = dynamic_cast<SplineMotion *>(entry->motion.get());
	if (!spline || selectedPoint_ >= static_cast<int>(spline->points.size())) return;

	const MotionContext ctx = MakePreviewContext();
	SplinePointGizmable handle(spline, selectedPoint_, &ctx);

	std::vector<IGizmable *> targets = {&handle};
	gizmo_.Draw(camera_, targets, Editor::GetInstance()->GetGameViewPos(),
	            Editor::GetInstance()->GetGameViewSize());
}

#endif // USE_IMGUI
