#include "UIBase.h"

// Engine
#ifdef USE_IMGUI
#include <imgui.h>
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
	if (ImGui::CollapsingHeader("アニメーション", ImGuiTreeNodeFlags_DefaultOpen)) {

		if (IsAnimating()) {
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "▶ アニメーション再生中...");
			ImGui::Text("アクティブなアニメーション数: %zu", animator_.GetAnimations().size());

			ImGui::Spacing();

			// 各アニメーションの進捗を表示
			const auto& anims = animator_.GetAnimations();
			for (size_t i = 0; i < anims.size(); ++i) {
				const UIAnimation& anim = anims[i];

				ImGui::PushID(static_cast<int>(i));

				// アニメーションタイプを表示
				const char* typeName = "Unknown";
				switch (anim.type) {
				case UIAnimationType::Position: typeName = "位置"; break;
				case UIAnimationType::Scale: typeName = "スケール"; break;
				case UIAnimationType::Rotation: typeName = "回転"; break;
				case UIAnimationType::Alpha: typeName = "アルファ"; break;
				case UIAnimationType::Color: typeName = "色"; break;
				case UIAnimationType::FadeIn: typeName = "フェードイン"; break;
				case UIAnimationType::FadeOut: typeName = "フェードアウト"; break;
				case UIAnimationType::SlideIn: typeName = "スライドイン"; break;
				case UIAnimationType::SlideOut: typeName = "スライドアウト"; break;
				case UIAnimationType::ZoomIn: typeName = "ズームイン"; break;
				case UIAnimationType::ZoomOut: typeName = "ズームアウト"; break;
				case UIAnimationType::Shake: typeName = "シェイク"; break;
				case UIAnimationType::Pulse: typeName = "パルス"; break;
				case UIAnimationType::Bounce: typeName = "バウンス"; break;
				case UIAnimationType::Swing: typeName = "スイング"; break;
				case UIAnimationType::Flash: typeName = "フラッシュ"; break;
				case UIAnimationType::Blink: typeName = "ブリンク"; break;
				case UIAnimationType::Wobble: typeName = "ウォブル"; break;
				case UIAnimationType::Flip: typeName = "フリップ"; break;
				case UIAnimationType::RotateIn: typeName = "回転イン"; break;
				case UIAnimationType::RotateOut: typeName = "回転アウト"; break;
				}

				ImGui::Text("%zu: %s", i + 1, typeName);

				float progress = anim.elapsed / anim.duration;
				ImGui::ProgressBar(progress, ImVec2(-1, 0));
				ImGui::Text("%.2f / %.2f秒", anim.elapsed, anim.duration);

				ImGui::PopID();
				ImGui::Spacing();
			}

			// 制御ボタン
			if (IsPaused()) {
				if (ImGui::Button("▶ 再開", ImVec2(100, 0))) {
					ResumeAnimation();
				}
			} else {
				if (ImGui::Button("⏸ 一時停止", ImVec2(100, 0))) {
					PauseAnimation();
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("■ 全停止", ImVec2(100, 0))) {
				StopAllAnimations();
			}

			ImGui::Separator();
		} else {
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "アニメーション停止中");
			ImGui::Separator();
		}

		ImGui::Spacing();

		// アニメーション設定
		static int selectedAnimType = 0;
		static int selectedEasing = 0;
		static float duration = 1.0f;
		static bool loop = false;
		static float intensity = 1.0f;

		ImGui::Text("📝 アニメーション設定");

		// アニメーションタイプ選択
		const char* animTypes[] = {
			"位置", "スケール", "回転", "アルファ", "色",
			"フェードイン", "フェードアウト",
			"スライドイン", "スライドアウト",
			"ズームイン", "ズームアウト",
			"シェイク", "パルス", "バウンス", "スイング",
			"フラッシュ", "ブリンク", "ウォブル", "フリップ",
			"回転イン", "回転アウト"
		};
		ImGui::Combo("タイプ", &selectedAnimType, animTypes, IM_ARRAYSIZE(animTypes));

		// イージング選択
		const char* easingTypes[] = {
			"Linear",
			"EaseInSine", "EaseOutSine", "EaseInOutSine",
			"EaseInQuad", "EaseOutQuad", "EaseInOutQuad",
			"EaseInCubic", "EaseOutCubic", "EaseInOutCubic",
			"EaseInQuart", "EaseOutQuart", "EaseInOutQuart",
			"EaseInQuint", "EaseOutQuint", "EaseInOutQuint",
			"EaseInExpo", "EaseOutExpo", "EaseInOutExpo",
			"EaseInCirc", "EaseOutCirc", "EaseInOutCirc",
			"EaseInBack", "EaseOutBack", "EaseInOutBack",
			"EaseInElastic", "EaseOutElastic", "EaseInOutElastic",
			"EaseInBounce", "EaseOutBounce", "EaseInOutBounce"
		};
		ImGui::Combo("イージング", &selectedEasing, easingTypes, IM_ARRAYSIZE(easingTypes));

		ImGui::DragFloat("時間(秒)", &duration, 0.1f, 0.1f, 10.0f);
		ImGui::Checkbox("ループ", &loop);

		// タイプ別のパラメータ
		if (selectedAnimType >= 11) { // エフェクト系
			ImGui::DragFloat("強度", &intensity, 0.1f, 0.1f, 100.0f);
		}

		ImGui::Spacing();

		// 再生ボタン
		if (ImGui::Button("▶ アニメーション再生", ImVec2(-1, 30))) {
			Easing::Function easing = static_cast<Easing::Function>(selectedEasing);

			Vector3 currentPos = GetPosition();
			Vector2 currentScale = GetScale();
			Vector3 currentRot = GetRotation();
			Vector4 currentColor = GetColor();
			float currentAlpha = GetAlpha();

			switch (selectedAnimType) {
			case 0: // 位置
				PlayPositionAnimation(currentPos,
					Vector3{ currentPos.x + 100.0f, currentPos.y + 50.0f, currentPos.z },
					duration, easing, loop);
				break;

			case 1: // スケール
				PlayScaleAnimation(currentScale,
					Vector2{ currentScale.x * 1.5f, currentScale.y * 1.5f },
					duration, easing, loop);
				break;

			case 2: // 回転
				PlayRotationAnimation(currentRot,
					Vector3{ 0, 0, currentRot.z + 360.0f },
					duration, easing, loop);
				break;

			case 3: // アルファ
				PlayAlphaAnimation(currentAlpha, 0.0f, duration, easing, loop);
				break;

			case 4: // 色
				PlayColorAnimation(currentColor,
					Vector4{ 1.0f, 0.0f, 0.0f, 1.0f }, duration, easing, loop);
				break;

			case 5: // フェードイン
				PlayFadeIn(duration, easing);
				break;

			case 6: // フェードアウト
				PlayFadeOut(duration, easing);
				break;

			case 7: // スライドイン
				PlaySlideIn(SlideDirection::Right, 200.0f, duration);
				break;

			case 8: // スライドアウト
				PlaySlideOut(SlideDirection::Left, 200.0f, duration);
				break;

			case 9: // ズームイン
				PlayZoomIn(duration);
				break;

			case 10: // ズームアウト
				PlayZoomOut(duration);
				break;

			case 11: // シェイク
				PlayShake(intensity * 10.0f, duration);
				break;

			case 12: // パルス
				PlayPulse(1.0f + intensity * 0.2f, duration, loop);
				break;

			case 13: // バウンス
				PlayBounce(intensity * 50.0f, duration);
				break;

			case 14: // スイング
				PlaySwing(intensity * 15.0f, duration, loop);
				break;

			case 15: // フラッシュ
				PlayFlash(duration, static_cast<int>(intensity * 3));
				break;

			case 16: // ブリンク
				PlayBlink(duration, loop);
				break;

			case 17: // ウォブル
				PlayWobble(intensity * 20.0f, duration);
				break;

			case 18: // フリップ
				PlayFlip(true, duration);
				break;

			case 19: // 回転イン
				PlayRotateIn(duration);
				break;

			case 20: // 回転アウト
				PlayRotateOut(duration);
				break;
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// プリセットボタン
		if (ImGui::TreeNode("🎬 プリセット")) {
			ImGui::Text("登場アニメーション:");

			if (ImGui::Button("フェードイン", ImVec2(120, 0))) {
				PlayFadeIn(0.8f, Easing::Function::EaseOutQuad);
			}
			ImGui::SameLine();
			if (ImGui::Button("ズームイン", ImVec2(120, 0))) {
				PlayZoomIn(0.6f);
			}
			ImGui::SameLine();
			if (ImGui::Button("回転イン", ImVec2(120, 0))) {
				PlayRotateIn(0.8f);
			}

			if (ImGui::Button("スライド←", ImVec2(120, 0))) {
				PlaySlideIn(SlideDirection::Right, 300.0f);
			}
			ImGui::SameLine();
			if (ImGui::Button("スライド→", ImVec2(120, 0))) {
				PlaySlideIn(SlideDirection::Left, 300.0f);
			}
			ImGui::SameLine();
			if (ImGui::Button("スライド↑", ImVec2(120, 0))) {
				PlaySlideIn(SlideDirection::Down, 300.0f);
			}

			ImGui::Spacing();
			ImGui::Text("退場アニメーション:");

			if (ImGui::Button("フェードアウト", ImVec2(120, 0))) {
				PlayFadeOut(0.8f, Easing::Function::EaseInQuad);
			}
			ImGui::SameLine();
			if (ImGui::Button("ズームアウト", ImVec2(120, 0))) {
				PlayZoomOut(0.6f);
			}
			ImGui::SameLine();
			if (ImGui::Button("回転アウト", ImVec2(120, 0))) {
				PlayRotateOut(0.8f);
			}

			ImGui::Spacing();
			ImGui::Text("エフェクト:");

			if (ImGui::Button("シェイク", ImVec2(120, 0))) {
				PlayShake(15.0f, 0.5f);
			}
			ImGui::SameLine();
			if (ImGui::Button("バウンス", ImVec2(120, 0))) {
				PlayBounce(80.0f, 1.2f);
			}
			ImGui::SameLine();
			if (ImGui::Button("フラッシュ", ImVec2(120, 0))) {
				PlayFlash(0.6f, 4);
			}

			if (ImGui::Button("パルス", ImVec2(120, 0))) {
				PlayPulse(1.3f, 0.6f, true);
			}
			ImGui::SameLine();
			if (ImGui::Button("スイング", ImVec2(120, 0))) {
				PlaySwing(20.0f, 0.8f, true);
			}
			ImGui::SameLine();
			if (ImGui::Button("ウォブル", ImVec2(120, 0))) {
				PlayWobble(25.0f, 1.0f);
			}

			ImGui::TreePop();
		}
	}
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
					} else if (entry.is_regular_file()) {
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
			} else {
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
		} else {
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
