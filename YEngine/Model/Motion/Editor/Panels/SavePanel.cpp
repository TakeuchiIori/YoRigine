#include "SavePanel.h"
#include "Model.h"
#include "Object3D/ObjectManager.h"
#include "../../Core/Motion.h"
#include "../../Core/MotionSystem.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace fs = std::filesystem;

namespace {
	std::string NormalizePathString(const fs::path& path)
	{
		return path.generic_string();
	}
}

// -----------------------------------------------------------------------
// Initialize
// -----------------------------------------------------------------------
void SavePanel::Initialize(MotionEditorContext* context)
{
	context_ = context;

#ifdef USE_IMGUI
	// FileBrowser を .anim フィルタ・リストモードで初期化
	fileBrowser_ = YoRigine::FileBrowser(
		"Resources/Binary",
		{ ".anim" },
		YoRigine::FileBrowser::DisplayMode::List
	);

	// ファイル選択時: パスを反映 → 即読み込み → ブラウザを閉じる
	fileBrowser_.SetOnFileSelected([this](const std::string& fullPath) {
		savePath_ = fullPath;
		browserOpen_ = false;
		LoadBinary(fullPath);
		});

	sourceFileBrowser_ = YoRigine::FileBrowser(
		"Resources/Models",
		{ ".gltf" },
		YoRigine::FileBrowser::DisplayMode::List
	);
	sourceFileBrowser_.SetOnFileSelected([this](const std::string& fullPath) {
		SelectSourceModel(fullPath);
		});
#endif
}

// -----------------------------------------------------------------------
// DrawImGui
// -----------------------------------------------------------------------
void SavePanel::DrawImGui()
{
#ifdef USE_IMGUI
	// ブラウザウィンドウが開いている間だけ描画
	if (browserOpen_) {
		DrawBrowserWindow();
	}
	if (sourceBrowserOpen_) {
		DrawSourceBrowserWindow();
	}

	DrawSourceAnimationPopup();
	DrawSaveLoadPopup();
#endif
}

// -----------------------------------------------------------------------
// DrawBrowserWindow  (FileBrowser を独立ウィンドウとして表示)
// -----------------------------------------------------------------------
#ifdef USE_IMGUI
void SavePanel::DrawBrowserWindow()
{
	ImGui::SetNextWindowSize(ImVec2(540, 420), ImGuiCond_FirstUseEver);
	bool open = true;
	if (!ImGui::Begin("バイナリファイルを選択##browser", &open)) {
		ImGui::End();
		if (!open) browserOpen_ = false;
		return;
	}

	// FileBrowser 本体を描画（Child 枠付き、高さを残してボタン分を確保）
	fileBrowser_.Draw("##animBrowser", ImVec2(0, -50));

	ImGui::Separator();

	// 現在の選択パスを表示
	const std::string& sel = fileBrowser_.GetSelectedPath();
	ImGui::TextUnformatted(sel.empty() ? "(ファイルが未選択です)" : sel.c_str());

	float btnX = ImGui::GetContentRegionAvail().x - 120.0f;
	ImGui::SameLine(btnX);

	bool canOK = !sel.empty();
	if (!canOK) ImGui::BeginDisabled();
	if (ImGui::Button("OK", ImVec2(55, 0))) {
		// OnFileSelected コールバックが既に savePath_ を更新しているが、
		// ダブルクリックではなく OK ボタンで確定する場合はここで同期する
		if (!sel.empty()) {
			savePath_ = sel;
			browserOpen_ = false;
		}
	}
	if (!canOK) ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button("キャンセル", ImVec2(60, 0))) {
		fileBrowser_.SetSelectedPath("");
		browserOpen_ = false;
	}

	if (!open) browserOpen_ = false;
	ImGui::End();
}
#endif

#ifdef USE_IMGUI
void SavePanel::DrawSourceBrowserWindow()
{
	ImGui::SetNextWindowSize(ImVec2(560, 430), ImGuiCond_FirstUseEver);
	bool open = true;
	if (!ImGui::Begin("元アニメーションファイルを選択##sourceBrowser", &open)) {
		ImGui::End();
		if (!open) sourceBrowserOpen_ = false;
		return;
	}

	sourceFileBrowser_.Draw("##sourceAnimBrowser", ImVec2(0, -50));

	ImGui::Separator();
	const std::string& sel = sourceFileBrowser_.GetSelectedPath();
	ImGui::TextUnformatted(sel.empty() ? "(ファイルが未選択です)" : sel.c_str());

	float btnX = ImGui::GetContentRegionAvail().x - 120.0f;
	ImGui::SameLine(btnX);

	bool canOK = !sel.empty();
	if (!canOK) ImGui::BeginDisabled();
	if (ImGui::Button("OK", ImVec2(55, 0))) {
		if (!sel.empty()) {
			SelectSourceModel(sel);
			sourceBrowserOpen_ = false;
			context_->showSourceAnimationPopup = true;
		}
	}
	if (!canOK) ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button("キャンセル", ImVec2(60, 0))) {
		sourceFileBrowser_.SetSelectedPath("");
		sourceBrowserOpen_ = false;
		context_->showSourceAnimationPopup = true;
	}

	if (!open) sourceBrowserOpen_ = false;
	ImGui::End();
}
#endif

// -----------------------------------------------------------------------
// DrawSaveLoadPopup
// -----------------------------------------------------------------------
void SavePanel::DrawSaveLoadPopup()
{
#ifdef USE_IMGUI
	if (context_->showSavePopup) ImGui::OpenPopup("保存 / 読み込み##popup");

	ImGui::SetNextWindowSize(ImVec2(500, 280), ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal("保存 / 読み込み##popup", &context_->showSavePopup)) {

		ImGui::Text("バイナリファイル (.anim):");
		{
			char buf[512];
			strncpy_s(buf, sizeof(buf), savePath_.c_str(), _TRUNCATE);
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90);
			if (ImGui::InputText("##savepath", buf, sizeof(buf))) savePath_ = buf;
			ImGui::SameLine();
			if (ImGui::Button("参照##b")) {
				// ブラウザを開く前に最新スキャン & 選択状態をリセット
				fileBrowser_.Scan();
				fileBrowser_.SetSelectedPath("");
				browserOpen_ = true;
				context_->showSavePopup = false;
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

		bool canSave = context_->currentMotion != nullptr;
		if (!canSave) ImGui::BeginDisabled();
		if (ImGui::Button("バイナリ保存", ImVec2(130, 0))) {
			try {
				if (context_->currentMotion) {
					if (context_->isReverse) {
						// 逆再生モード: キーフレームの時間を反転したコピーを保存
						Motion reversed = *context_->currentMotion;
						float duration = reversed.GetDuration();
						for (auto& [name, na] : reversed.animation_.nodeAnimations_) {
							for (auto& kf : na.translate.keyframes) kf.time = duration - kf.time;
							for (auto& kf : na.rotate.keyframes)    kf.time = duration - kf.time;
							for (auto& kf : na.scale.keyframes)     kf.time = duration - kf.time;
							std::sort(na.translate.keyframes.begin(), na.translate.keyframes.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
							std::sort(na.rotate.keyframes.begin(), na.rotate.keyframes.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
							std::sort(na.scale.keyframes.begin(), na.scale.keyframes.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
						}
						reversed.SaveBinary(reversed, AnimDisplayName(context_->selectedAnimKey), savePath_);
						saveMsg_ = "保存成功 (逆再生): " + savePath_;
					}
					else {
						context_->currentMotion->SaveBinary(
							*context_->currentMotion,
							AnimDisplayName(context_->selectedAnimKey),
							savePath_
						);
						saveMsg_ = "保存成功: " + savePath_;
					}
					context_->statusMsg = saveMsg_;
				}
			}
			catch (const std::exception& e) {
				saveMsg_ = std::string("保存失敗: ") + e.what();
			}
		}
		if (!canSave) ImGui::EndDisabled();

		ImGui::SameLine(0, 10);

		if (ImGui::Button("バイナリ読み込み", ImVec2(140, 0))) {
			LoadBinary(savePath_);
		}


		if (!saveMsg_.empty()) {
			ImGui::Spacing();
			bool isError = saveMsg_.find("失敗") != std::string::npos;
			ImGui::TextColored(
				isError ? ImVec4(1, 0.3f, 0.3f, 1) : ImVec4(0.3f, 1, 0.3f, 1),
				"%s", saveMsg_.c_str()
			);
		}

		ImGui::Spacing(); ImGui::Separator();
		if (ImGui::Button("閉じる", ImVec2(100, 0))) {
			context_->showSavePopup = false;
			saveMsg_ = "";
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
	if (!context_->showSavePopup) ImGui::CloseCurrentPopup();
#endif
}

void SavePanel::DrawSourceAnimationPopup()
{
#ifdef USE_IMGUI
	if (context_->showSourceAnimationPopup) ImGui::OpenPopup("元アニメーション読み込み##popup");

	ImGui::SetNextWindowSize(ImVec2(560, 330), ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal("元アニメーション読み込み##popup", &context_->showSourceAnimationPopup)) {
		ImGui::Text("モデルファイル (.gltf):");
		{
			char buf[512];
			strncpy_s(buf, sizeof(buf), sourceModelPath_.c_str(), _TRUNCATE);
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90);
			if (ImGui::InputText("##sourceModelPath", buf, sizeof(buf))) {
				sourceModelPath_ = buf;
				sourceObjectPath_ = ToObjectModelPath(sourceModelPath_);
				sourceAnimationNames_.clear();
			}
			ImGui::SameLine();
			if (ImGui::Button("参照##source")) {
				sourceFileBrowser_.Scan();
				sourceFileBrowser_.SetSelectedPath("");
				sourceBrowserOpen_ = true;
				context_->showSourceAnimationPopup = false;
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::Spacing();
		if (ImGui::Button("候補取得", ImVec2(100, 0))) {
			SelectSourceModel(sourceModelPath_);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%s", sourceObjectPath_.empty() ? "" : sourceObjectPath_.c_str());

		ImGui::Spacing();
		ImGui::Text("アニメーション:");
		if (sourceAnimationNames_.empty()) {
			ImGui::TextDisabled("候補なし");
		}
		else {
			const char* preview = sourceAnimationName_.empty() ? "(未選択)" : sourceAnimationName_.c_str();
			if (ImGui::BeginCombo("##sourceAnimationName", preview)) {
				for (const auto& name : sourceAnimationNames_) {
					bool selected = (name == sourceAnimationName_);
					if (ImGui::Selectable(name.c_str(), selected)) sourceAnimationName_ = name;
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		char animBuf[256];
		strncpy_s(animBuf, sizeof(animBuf), sourceAnimationName_.c_str(), _TRUNCATE);
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		if (ImGui::InputText("名前を直接入力##sourceAnimInput", animBuf, sizeof(animBuf))) {
			sourceAnimationName_ = animBuf;
		}

		ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

		const bool canLoad = !sourceObjectPath_.empty() && !sourceAnimationName_.empty() && context_->GetTargetObject();
		if (!canLoad) ImGui::BeginDisabled();
		if (ImGui::Button("読み込んで適用", ImVec2(140, 0))) {
			LoadSourceAnimation();
		}
		if (!canLoad) ImGui::EndDisabled();

		ImGui::SameLine();
		if (ImGui::Button("閉じる", ImVec2(100, 0))) {
			context_->showSourceAnimationPopup = false;
			sourceMsg_ = "";
			ImGui::CloseCurrentPopup();
		}

		if (!sourceMsg_.empty()) {
			ImGui::Spacing();
			bool isError = sourceMsg_.find("失敗") != std::string::npos;
			ImGui::TextColored(
				isError ? ImVec4(1, 0.3f, 0.3f, 1) : ImVec4(0.3f, 1, 0.3f, 1),
				"%s", sourceMsg_.c_str()
			);
		}

		ImGui::EndPopup();
	}
	if (!context_->showSourceAnimationPopup) ImGui::CloseCurrentPopup();
#endif
}

// -----------------------------------------------------------------------
// AnimDisplayName
// -----------------------------------------------------------------------
std::string SavePanel::AnimDisplayName(const std::string& key)
{
	auto pos = key.find('#');
	return (pos != std::string::npos) ? key.substr(pos + 1) : key;
}

std::string SavePanel::ToObjectModelPath(const std::string& fullPath)
{
	fs::path path(fullPath);
	fs::path rel = path;

	if (path.is_absolute()) {
		std::error_code ec;
		rel = fs::relative(path, fs::absolute("Resources/Models", ec), ec);
		if (ec) rel = path.filename();
	}
	else {
		rel = path.lexically_normal();
		auto it = rel.begin();
		if (it != rel.end() && it->generic_string() == "Resources") {
			++it;
			if (it != rel.end() && it->generic_string() == "Models") {
				fs::path stripped;
				for (++it; it != rel.end(); ++it) stripped /= *it;
				rel = stripped;
			}
		}
	}

	if (rel.has_parent_path() && rel.parent_path().filename() == rel.stem()) {
		rel = rel.parent_path().parent_path() / rel.filename();
	}

	return NormalizePathString(rel);
}

std::string SavePanel::ToModelFilePathFromObjectPath(const std::string& objectPath)
{
	fs::path objectRel(objectPath);
	fs::path base = objectRel;
	base.replace_extension("");
	return NormalizePathString(fs::path("Resources/Models") / base / objectRel.filename());
}

// -----------------------------------------------------------------------
// LoadBinary  (ファイル選択・ボタン共通の読み込み処理)
// -----------------------------------------------------------------------
void SavePanel::LoadBinary(const std::string& path)
{
	try {
		Motion loaded = Motion().LoadBinary(path);
		const std::string key = "Binary:" + path;
		Model::animationCache_[key] = std::move(loaded);
		context_->selectedAnimKey = key;
		context_->currentMotion = &Model::animationCache_[key];

		// MotionSystem のアニメーションポインタも同期
		Object3d* target = context_->GetTargetObject();
		if (target && target->GetModel() && target->GetModel()->GetMotionSystem()) {
			auto* ms = target->GetModel()->GetMotionSystem();
			ms->SetAnimation(context_->currentMotion);
			ms->SetAnimationTime(0.0f);
			ms->SetPlayMode(MotionPlayMode::Loop);
		}

		saveMsg_ = "読み込み成功: " + path;
		context_->statusMsg = saveMsg_;
		context_->requireTimelineRebuild = true;
		context_->lastAppliedScrubTime = -1.0f;
		context_->scrubTime = 0.0f;
	}
	catch (const std::exception& e) {
		saveMsg_ = std::string("読み込み失敗: ") + e.what();
	}
}

void SavePanel::SelectSourceModel(const std::string& path)
{
	sourceModelPath_ = path;
	sourceObjectPath_ = ToObjectModelPath(path);
	sourceAnimationNames_.clear();
	sourceAnimationName_.clear();
	std::string readPath = path;
	if (!fs::exists(readPath)) {
		readPath = ToModelFilePathFromObjectPath(sourceObjectPath_);
	}

	try {
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(readPath.c_str(), 0);
		if (!scene || scene->mNumAnimations == 0) {
			sourceMsg_ = "アニメーションが見つかりません: " + readPath;
			return;
		}

		sourceAnimationNames_.reserve(scene->mNumAnimations);
		for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
			std::string name = scene->mAnimations[i]->mName.C_Str();
			if (name.empty()) name = "Animation" + std::to_string(i);
			sourceAnimationNames_.push_back(name);
		}

		sourceAnimationName_ = sourceAnimationNames_.front();
		sourceMsg_ = "候補取得: " + std::to_string(sourceAnimationNames_.size()) + "件";
	}
	catch (const std::exception& e) {
		sourceMsg_ = std::string("候補取得失敗: ") + e.what();
	}
}

void SavePanel::LoadSourceAnimation()
{
	try {
		Object3d* target = context_->GetTargetObject();
		if (!target || !target->GetModel()) {
			sourceMsg_ = "読み込み失敗: 対象オブジェクトがありません";
			return;
		}

		target->SetChangeMotion(sourceObjectPath_, MotionPlayMode::Loop, sourceAnimationName_);

		auto* model = target->GetModel();
		auto* ms = model ? model->GetMotionSystem() : nullptr;
		if (!ms || !ms->GetAnimation()) {
			sourceMsg_ = "読み込み失敗: MotionSystemに同期できません";
			return;
		}

		context_->loadFileName = sourceObjectPath_;
		context_->selectedAnimKey = ToModelFilePathFromObjectPath(sourceObjectPath_) + "#" + sourceAnimationName_;
		context_->currentMotion = ms->GetAnimation();
		context_->scrubTime = 0.0f;
		context_->lastAppliedScrubTime = -1.0f;
		context_->isPlaying = true;
		context_->requireTimelineRebuild = true;
		context_->selKF.Clear();

		sourceMsg_ = "読み込み成功: " + sourceObjectPath_ + " / " + sourceAnimationName_;
		context_->statusMsg = sourceMsg_;
	}
	catch (const std::exception& e) {
		sourceMsg_ = std::string("読み込み失敗: ") + e.what();
		context_->statusMsg = sourceMsg_;
	}
}
