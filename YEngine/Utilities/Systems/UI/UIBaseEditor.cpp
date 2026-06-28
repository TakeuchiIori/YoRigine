#include "UIBase.h"

// Engine
#ifdef USE_IMGUI
#include <imgui.h>
#include "Debugger/DopeSheet/DopeSheetEditor.h"
#endif

// C++
#include <filesystem>
#include <functional>
#include <algorithm>
#include <cctype>
#include <cstring>

// ============================================================
// UIBase の ImGui エディタ部
// ============================================================
// UIBase 本体（実行時ロジック）から ImGui パネル群を分離したファイル。
// 各関数は UIBase のメンバ定義のままなので private へ自由にアクセスできる。
// 本体は内部 #ifdef USE_IMGUI でガードしているため Release では空関数になる。
// ============================================================

// ============================================================
// グリッド設定パネル
// ============================================================
void UIBase::ImGuiGridSettings() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("グリッド設定")) {
		ImGui::Checkbox("グリッドを有効化", &gridEnabled_);

		if (gridEnabled_) {
			ImGui::DragFloat("グリッドサイズ", &gridSize_, 1.0f, 1.0f, 100.0f);

			if (ImGui::Button("位置をグリッドにスナップ")) {
				SetPosition(SnapToGrid(GetPosition()));
			}
		}
	}
#endif
}

/*==================================================================
					ImGui拡張アニメーション設定
===================================================================*/

void UIBase::ImGuiAnimationSettings() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("アニメーション", ImGuiTreeNodeFlags_DefaultOpen)) return;

	// ============================================================
	// 再生状態ヘッダ
	// ============================================================
	if (IsAnimating()) {
		ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "▶ 再生中");
		ImGui::SameLine();
		if (IsPaused()) {
			if (ImGui::SmallButton("再開")) ResumeAnimation();
		}
		else {
			if (ImGui::SmallButton("一時停止")) PauseAnimation();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("全停止")) StopAllAnimations();
	}
	else {
		ImGui::TextDisabled("停止中");
	}

	ImGui::Separator();
	ImGui::Spacing();

	// ============================================================
	// イージング名テーブル（クリップ編集・キーフレーム編集で共用）
	// ============================================================
	static const char* kEasingNames[] = {
		"Linear",
		"EaseInSine",    "EaseOutSine",    "EaseInOutSine",
		"EaseInQuad",    "EaseOutQuad",    "EaseInOutQuad",
		"EaseInCubic",   "EaseOutCubic",   "EaseInOutCubic",
		"EaseInQuart",   "EaseOutQuart",   "EaseInOutQuart",
		"EaseInQuint",   "EaseOutQuint",   "EaseInOutQuint",
		"EaseInExpo",    "EaseOutExpo",    "EaseInOutExpo",
		"EaseInCirc",    "EaseOutCirc",    "EaseInOutCirc",
		"EaseInBack",    "EaseOutBack",    "EaseInOutBack",
		"EaseInElastic", "EaseOutElastic", "EaseInOutElastic",
		"EaseInBounce",  "EaseOutBounce",  "EaseInOutBounce",
	};
	static const int kEasingCount = IM_ARRAYSIZE(kEasingNames);

	static const char* kTriggerNames[] = { "手動", "OnInit（初期化時）", "OnShow（表示時）" };

	// トラックタイプ名テーブル
	static const char* kTrackTypeNames[] = {
		"Alpha", "Position X", "Position Y", "Scale X", "Scale Y", "Rotation Z",
		"Color R", "Color G", "Color B"
	};
	static const int kTrackTypeCount = IM_ARRAYSIZE(kTrackTypeNames);

	// ============================================================
	// クリップ一覧 + 選択
	// ============================================================
	// 選択中クリップインデックス（static で画面をまたいで保持）
	static int selectedClipIdx = -1;
	if (selectedClipIdx >= static_cast<int>(clips_.size())) selectedClipIdx = -1;

	ImGui::Text("💾 クリップ一覧");

	// クリップリスト
	{
		float listH = std::min(static_cast<float>(clips_.size()) * 22.0f + 10.0f, 120.0f);
		ImGui::BeginChild("##ClipList", ImVec2(-1, listH), true);
		for (int i = 0; i < static_cast<int>(clips_.size()); ++i) {
			const auto& c = clips_[i];
			bool selected = (i == selectedClipIdx);
			std::string label = c.name + "  [" + kTriggerNames[static_cast<int>(c.trigger)] + "]";
			if (ImGui::Selectable(label.c_str(), selected))
				selectedClipIdx = i;
		}
		ImGui::EndChild();
	}

	// 新規クリップ追加
	{
		static char newClipName[128] = "NewClip";
		ImGui::SetNextItemWidth(180);
		ImGui::InputText("##NewClipName", newClipName, sizeof(newClipName));
		ImGui::SameLine();
		if (ImGui::Button("＋ クリップ追加")) {
			UIAnimationClip clip;
			clip.name = (strlen(newClipName) > 0) ? newClipName : "Clip";
			clips_.push_back(clip);
			selectedClipIdx = static_cast<int>(clips_.size()) - 1;
		}
		if (selectedClipIdx >= 0) {
			ImGui::SameLine();
			if (ImGui::Button("削除")) {
				clips_.erase(clips_.begin() + selectedClipIdx);
				selectedClipIdx = -1;
			}
		}
	}

	if (selectedClipIdx < 0 || selectedClipIdx >= static_cast<int>(clips_.size())) {
		ImGui::TextDisabled("クリップを選択または追加してください");
		return;
	}

	ImGui::Separator();
	ImGui::Spacing();

	// ============================================================
	// 選択中クリップの編集
	// ============================================================
	UIAnimationClip& clip = clips_[selectedClipIdx];

	ImGui::PushID(selectedClipIdx);

	// --- 基本プロパティ ---
	{
		char nameBuf[128];
		strncpy_s(nameBuf, clip.name.c_str(), sizeof(nameBuf) - 1);
		nameBuf[sizeof(nameBuf) - 1] = '\0';
		if (ImGui::InputText("名前", nameBuf, sizeof(nameBuf)))
			clip.name = nameBuf;
	}
	ImGui::DragFloat("長さ(秒)", &clip.duration, 0.05f, 0.05f, 60.0f);
	{
		int trig = static_cast<int>(clip.trigger);
		if (ImGui::Combo("トリガ", &trig, kTriggerNames, IM_ARRAYSIZE(kTriggerNames)))
			clip.trigger = static_cast<UIAnimTrigger>(trig);
	}
	ImGui::Checkbox("ループ", &clip.loop);

	// 再生ボタン
	ImGui::SameLine();
	if (ImGui::Button("▶ 再生")) PlayClip(clip);
	ImGui::SameLine();
	if (ImGui::Button("■ 停止")) StopClip(clip.name);

	ImGui::Spacing();
	ImGui::Separator();

	// ============================================================
	// トラック管理
	// ============================================================
	ImGui::Text("📊 トラック");

	// トラック追加
	{
		static int newTrackType = 0;
		ImGui::SetNextItemWidth(140);
		ImGui::Combo("##NewTrackType", &newTrackType, kTrackTypeNames, kTrackTypeCount);
		ImGui::SameLine();
		if (ImGui::Button("＋ トラック追加")) {
			auto type = static_cast<UIAnimTrackType>(newTrackType);
			// 同タイプが既になければ追加
			clip.AddTrack(type);
		}
	}

	// トラック一覧
	int removeTrackIdx = -1;
	for (int ti = 0; ti < static_cast<int>(clip.tracks.size()); ++ti) {
		UIAnimTrack& track = clip.tracks[ti];
		ImGui::PushID(ti);

		bool open = ImGui::TreeNodeEx(
			UIAnimTrackTypeLabel(track.type),
			ImGuiTreeNodeFlags_DefaultOpen
		);

		// トラック削除ボタン（同じ行右側）
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40);
		if (ImGui::SmallButton("削除")) removeTrackIdx = ti;

		if (open) {
			// キーフレーム追加
			{
				static float newKeyTime = 0.0f;
				static float newKeyval = 0.0f;
				static int   newKeyEasing = 0;
				ImGui::SetNextItemWidth(70); ImGui::DragFloat("t##nkt", &newKeyTime, 0.01f, 0.0f, 1.0f);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(70); ImGui::DragFloat("v##nkv", &newKeyval, 0.01f);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(110);
				ImGui::Combo("##nke", &newKeyEasing, kEasingNames, kEasingCount);
				ImGui::SameLine();
				if (ImGui::SmallButton("＋キー追加")) {
					track.AddKey(newKeyTime, newKeyval,
						static_cast<Easing::Function>(newKeyEasing));
				}
			}

			// キーフレームテーブル
			if (!track.keyframes.empty()) {
				ImGui::BeginChild(
					ImGui::GetID("##keyframes"),
					ImVec2(-1, std::min(static_cast<float>(track.keyframes.size()) * 22.0f + 8.0f, 110.0f)),
					true
				);

				int removeKeyIdx = -1;
				for (int ki = 0; ki < static_cast<int>(track.keyframes.size()); ++ki) {
					UIAnimKeyframe& key = track.keyframes[ki];
					ImGui::PushID(ki);

					ImGui::SetNextItemWidth(70);
					if (ImGui::DragFloat("##t", &key.time, 0.005f, 0.0f, 1.0f))
						track.Sortkeyframes();  // 時間変更後ソート
					ImGui::SameLine();
					ImGui::SetNextItemWidth(70);
					ImGui::DragFloat("##v", &key.val, 0.01f);
					ImGui::SameLine();
					ImGui::SetNextItemWidth(110);
					int e = static_cast<int>(key.easing);
					if (ImGui::Combo("##e", &e, kEasingNames, kEasingCount))
						key.easing = static_cast<Easing::Function>(e);
					ImGui::SameLine();
					if (ImGui::SmallButton("✕")) removeKeyIdx = ki;

					ImGui::PopID();
				}
				if (removeKeyIdx >= 0) track.RemoveKey(removeKeyIdx);

				ImGui::EndChild();
			}
			else {
				ImGui::TextDisabled("  キーフレームがありません");
			}

			// DopeSheetEditor でキーを視覚的に確認・移動
			{
				// UIAnimTrack → DopeTrack に変換して描画
				static DopeSheet::DopeSheetEditor dopeEditor;
				std::vector<DopeSheet::DopeTrack> dopeTracks(1);
				dopeTracks[0].label = UIAnimTrackTypeLabel(track.type);
				dopeTracks[0].color = DopeSheet::Color::Cyan();
				// 正規化時間 [0,1] → フレーム換算（60fps想定）
				const int kFps = 60;
				const int totalFrames = static_cast<int>(clip.duration * kFps);
				for (const auto& k : track.keyframes) {
					int frame = static_cast<int>(k.time * totalFrames);
					dopeTracks[0].AddKey(frame, k.val);
				}

				// DopeSheet の変更コールバックを設定して描画
				bool moved = false;
				dopeEditor.SetMoveKeyCallback([&](int /*trackIdx*/, int keyIdx, int newFrame) {
					if (keyIdx >= 0 && keyIdx < static_cast<int>(track.keyframes.size())) {
						track.keyframes[keyIdx].time = static_cast<float>(newFrame) / totalFrames;
						track.Sortkeyframes();
						moved = true;
					}
					});
				dopeEditor.SetAddKeyCallback([&](int /*trackIdx*/, int frame, float val) {
					float t = static_cast<float>(frame) / totalFrames;
					track.AddKey(t, val);
					});
				dopeEditor.SetDeleteKeyCallback([&](int /*trackIdx*/, int keyIdx) {
					track.RemoveKey(keyIdx);
					});
				dopeEditor.Draw(
					"##dope",
					dopeTracks,
					totalFrames,
					kFps,
					60.0f
				);
				(void)moved;
			}

			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	if (removeTrackIdx >= 0)
		clip.tracks.erase(clip.tracks.begin() + removeTrackIdx);

	ImGui::PopID();

	ImGui::Spacing();
	ImGui::TextDisabled("※ 「変更を保存」で JSON に永続化されます");
#endif
}

// ============================================================
// プリセット保存/読込パネル
// ============================================================
void UIBase::ImGuiPresetSettings() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("プリセット")) {
		static char presetName[128] = "";
		ImGui::InputText("プリセット名", presetName, sizeof(presetName));

		if (ImGui::Button("現在の設定を保存")) {
			if (strlen(presetName) > 0) {
				if (SaveAsPreset(presetName)) {
					ImGui::OpenPopup("PresetSaved");
				}
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("プリセットから読み込み")) {
			ImGui::OpenPopup("LoadPresetPopup");
		}

		if (ImGui::BeginPopupModal("PresetSaved", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("プリセットを保存しました!");
			if (ImGui::Button("OK")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("LoadPresetPopup")) {
			ImGui::Text("プリセットを選択:");
			ImGui::Separator();

			auto presets = GetAvailablePresets();
			for (const auto& preset : presets) {
				if (ImGui::Selectable(preset.c_str())) {
					LoadPreset(preset);
					ImGui::CloseCurrentPopup();
				}
			}

			if (presets.empty()) {
				ImGui::TextDisabled("プリセットがありません");
			}

			ImGui::EndPopup();
		}
	}
#endif
}

// ============================================================
// クイック配置パネル
// ============================================================
void UIBase::ImGuiQuickAlignment() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("クイック配置")) {
		ImGui::Text("画面位置:");

		if (ImGui::Button("左上")) {
			SetPosition({ 0.0f, 0.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("中央上")) {
			SetPosition({ 640.0f, 0.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("右上")) {
			SetPosition({ 1280.0f, 0.0f, GetPosition().z });
		}

		if (ImGui::Button("左中央")) {
			SetPosition({ 0.0f, 360.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("中央")) {
			SetPosition({ 640.0f, 360.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("右中央")) {
			SetPosition({ 1280.0f, 360.0f, GetPosition().z });
		}

		if (ImGui::Button("左下")) {
			SetPosition({ 0.0f, 720.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("中央下")) {
			SetPosition({ 640.0f, 720.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("右下")) {
			SetPosition({ 1280.0f, 720.0f, GetPosition().z });
		}
	}
#endif
}

// ============================================================
// UV SRT パネル
// ============================================================
void UIBase::ImGuiUVSRTSettings() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("UV SRT")) {
		// UV Translation
		Vector2 uvTrans = GetUVTranslation();
		if (ImGui::DragFloat2("UV Translation", &uvTrans.x, 0.01f, -10.0f, 10.0f)) {
			SetUVTranslation(uvTrans);
		}

		// UV Rotation
		float uvRot = GetUVRotation();
		if (ImGui::DragFloat("UV Rotation", &uvRot, 0.01f, -3.14159f * 2.0f, 3.14159f * 2.0f)) {
			SetUVRotation(uvRot);
		}

		// UV Scale
		Vector2 uvSc = GetUVScale();
		if (ImGui::DragFloat2("UV Scale", &uvSc.x, 0.01f, -10.0f, 10.0f)) {
			SetUVScale(uvSc);
		}

		ImGui::Separator();

		// リセットボタン
		if (ImGui::Button("UVリセット")) {
			SetUVTranslation({ 0.0f, 0.0f });
			SetUVRotation(0.0f);
			SetUVScale({ 1.0f, 1.0f });
		}

		ImGui::SameLine();

		// プリセットボタン
		if (ImGui::Button("UV反転X")) {
			SetUVScale({ -GetUVScale().x, GetUVScale().y });
		}

		ImGui::SameLine();

		if (ImGui::Button("UV反転Y")) {
			SetUVScale({ GetUVScale().x, -GetUVScale().y });
		}

		// UV Tiling プリセット
		ImGui::Text("UV Tiling:");
		if (ImGui::Button("1x1")) {
			SetUVScale({ 1.0f, 1.0f });
		}
		ImGui::SameLine();
		if (ImGui::Button("2x2")) {
			SetUVScale({ 2.0f, 2.0f });
		}
		ImGui::SameLine();
		if (ImGui::Button("4x4")) {
			SetUVScale({ 4.0f, 4.0f });
		}
	}
#endif
}

// ============================================================
// UI 1個分の総合インスペクタ
// ============================================================
void UIBase::ImGUi() {
#ifdef USE_IMGUI
	if (!sprite_) return;


	bool modified = false;

	char nameBuffer[256];
	strncpy_s(nameBuffer, name_.c_str(), sizeof(nameBuffer) - 1);
	nameBuffer[sizeof(nameBuffer) - 1] = '\0';
	if (ImGui::InputText("名前", nameBuffer, sizeof(nameBuffer))) {
		name_ = nameBuffer;
		modified = true;
	}

	if (ImGui::CollapsingHeader("トランスフォーム", ImGuiTreeNodeFlags_DefaultOpen)) {
		// 基準サイズ(px) … レイアウト用
		Vector2 size = GetSize();
		if (ImGui::DragFloat2("サイズ(px)", &size.x, 0.5f)) {
			SetSize(size);
			modified = true;
		}

		// 拡縮倍率(1.0=等倍) … アニメーション/演出用
		Vector2 scale = GetScale();
		if (ImGui::DragFloat2("拡大縮小(倍率)", &scale.x, 0.01f)) {
			SetScale(scale);
			modified = true;
		}

		Vector3 rotation = GetRotation();
		if (ImGui::DragFloat3("回転", &rotation.x, 0.1f)) {
			SetRotation(rotation);
			modified = true;
		}

		Vector3 position = GetPosition();
		if (ImGui::DragFloat3("位置", &position.x, 1.0f)) {
			SetPosition(position);
			modified = true;
		}
	}

	if (ImGui::CollapsingHeader("マテリアル", ImGuiTreeNodeFlags_DefaultOpen)) {
		Vector4 color = GetColor();
		if (ImGui::ColorEdit4("色", &color.x)) {
			SetColor(color);
			modified = true;
		}

		bool flipX = GetFlipX();
		if (ImGui::Checkbox("X軸反転", &flipX)) {
			SetFlipX(flipX);
			modified = true;
		}

		ImGui::SameLine();

		bool flipY = GetFlipY();
		if (ImGui::Checkbox("Y軸反転", &flipY)) {
			SetFlipY(flipY);
			modified = true;
		}
	}

	if (ImGui::CollapsingHeader("テクスチャ", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("現在のテクスチャ: %s", texturePath_.c_str());

		// 🔍 フィルタ入力
		static char textureFilter[128] = "";
		ImGui::InputTextWithHint("##filter", "ファイル名で検索...", textureFilter, sizeof(textureFilter));

		if (ImGui::Button("テクスチャを変更")) {
			ImGui::OpenPopup("TextureSelectPopup");
		}

		if (ImGui::BeginPopup("TextureSelectPopup")) {
			ImGui::Text("📁 画像を選択:");
			ImGui::Separator();

			std::string baseDir = "./Resources/Textures/";
			std::function<void(const std::filesystem::path&)> DrawFolderTree;

			DrawFolderTree = [&](const std::filesystem::path& folder) {
				for (const auto& entry : std::filesystem::directory_iterator(folder)) {
					if (entry.is_directory()) {
						// フォルダ表示（アイコン付き）
						std::string folderName = "📂 " + entry.path().filename().string();
						if (ImGui::TreeNode(folderName.c_str())) {
							DrawFolderTree(entry.path());
							ImGui::TreePop();
						}
					}
					else if (entry.is_regular_file()) {
						auto ext = entry.path().extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(),
							[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

						if (ext == ".png" || ext == ".jpg" || ext == ".dds") {
							std::string filename = entry.path().filename().string();

							// 検索フィルタ適用
							if (strlen(textureFilter) > 0 && filename.find(textureFilter) == std::string::npos) {
								continue;
							}

							std::string displayName = filename;
							bool isCurrent = (texturePath_ == entry.path().string());

							// 現在のテクスチャを強調表示
							if (isCurrent) {
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
							}

							if (ImGui::Selectable(displayName.c_str(), isCurrent, ImGuiSelectableFlags_AllowDoubleClick)) {
								std::string fullPath = entry.path().string();
								SetTexture(fullPath);
								ImGui::CloseCurrentPopup();
							}

							if (isCurrent) {
								ImGui::SameLine();
								ImGui::TextDisabled("（使用中）");
								ImGui::PopStyleColor();
							}
						}
					}
				}
				};

			if (std::filesystem::exists(baseDir)) {
				DrawFolderTree(baseDir);
			}
			else {
				ImGui::TextDisabled("Resources/images/ が存在しません。");
			}

			ImGui::EndPopup();
		}

		Vector2 leftTop = sprite_->GetTextureLeftTop();
		if (ImGui::DragFloat2("左上座標", &leftTop.x, 1.0f)) {
			sprite_->SetTextureLeftTop(leftTop);
			modified = true;
		}

		Vector2 textureSize = sprite_->GetTextureSize();
		if (ImGui::DragFloat2("テクスチャサイズ", &textureSize.x, 1.0f)) {
			sprite_->SetTextureSize(textureSize);
			modified = true;
		}

		Vector2 anchor = sprite_->GetAnchorPoint();
		if (ImGui::DragFloat2("アンカーポイント", &anchor.x, 0.01f, 0.0f, 1.0f)) {
			sprite_->SetAnchorPoint(anchor);
			modified = true;
		}
	}

	ImGuiGridSettings();
	ImGuiAnimationSettings();
	ImGuiPresetSettings();
	ImGuiQuickAlignment();
	ImGuiUVSRTSettings();

	if (ImGui::CollapsingHeader("表示設定")) {
		ImGui::Checkbox("表示", &visible_);
		ImGui::DragInt("レイヤー", &layer_, 1.0f, 0, 100);
	}

	bool hotReload = hotReloadEnabled_;
	if (ImGui::Checkbox("ホットリロード", &hotReload)) {
		EnableHotReload(hotReload);
	}

	ImGui::Separator();

	if (ImGui::Button("変更を保存")) {
		if (SaveToJSON()) {
			ImGui::OpenPopup("SaveSuccessPopup");
		}
		else {
			ImGui::OpenPopup("SaveFailedPopup");
		}
	}

	if (ImGui::BeginPopupModal("SaveSuccessPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("設定が正常に保存されました。");
		if (ImGui::Button("OK")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("SaveFailedPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("設定の保存に失敗しました。");
		if (ImGui::Button("OK")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (modified) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "* 未保存の変更があります");
	}

#endif
}
