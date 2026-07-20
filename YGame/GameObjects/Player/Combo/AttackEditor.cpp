#include "AttackEditor.h"
#include <algorithm>
#include <filesystem>
#include <map>

#include <Debugger/Logger.h>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

// ============================================================
// コンストラクタ
// ============================================================
AttackDataEditor::AttackDataEditor()
{
	attacks_ = &AttackDatabase::Get();
	std::fill(std::begin(nameBuffer_), std::end(nameBuffer_), '\0');
	RefreshPlayerAnimationList();
}

// ============================================================
// 編集対象のリストを設定
// ============================================================
void AttackDataEditor::SetTarget(std::vector<AttackData>* list)
{
	attacks_ = list;
	currentIndex_ = (attacks_ && !attacks_->empty()) ? 0 : -1;
	prevIndex_ = -1;
}

// ============================================================
// ファイルパスの設定
// ============================================================
void AttackDataEditor::SetFilePath(const std::string& path)
{
	filePath_ = path;
	Logger(("[AttackEditor] File path: " + filePath_ + "\n").c_str());
}

// ============================================================
// リロード時のコールバック設定
// ============================================================
void AttackDataEditor::SetReloadCallback(std::function<void()> callback)
{
	onReloadCallback_ = callback;
}

// ============================================================
// エディタUI全体の描画
// ============================================================
void AttackDataEditor::DrawImGui()
{
#ifdef USE_IMGUI
	// ------------------------------------------------------------
	// ツールバー領域の描画
	// ------------------------------------------------------------
	DrawToolbar();
	ImGui::Separator();

	// ------------------------------------------------------------
	// 上段カラム：リスト（左）と詳細プロパティ（右）
	// ------------------------------------------------------------
	ImGui::Columns(2, nullptr, true);
	DrawAttackList();
	ImGui::NextColumn();
	DrawAttackDetail();
	ImGui::Columns(1);

	ImGui::Separator();

	// ------------------------------------------------------------
	// 下段カラム：ドープシートによるタイムライン編集
	// ------------------------------------------------------------
	DrawDopeSheet();
#endif
}

// ============================================================
// ツールバーの描画
// ============================================================
void AttackDataEditor::DrawToolbar()
{
#ifdef USE_IMGUI
	if (ImGui::Button("保存")) { SaveToJson(); TriggerReload(); }
	ImGui::SameLine();
	if (ImGui::Button("読み込み")) { LoadFromJson(); TriggerReload(); }
	ImGui::SameLine();
	if (ImGui::Button("保存 & リロード")) { SaveToJson(); TriggerReload(); }
	ImGui::SameLine();
	if (ImGui::Button("アニメ一覧更新")) { RefreshPlayerAnimationList(); }

	ImGui::SameLine();
	ImGui::Text("ファイル: %s", filePath_.c_str());
	ImGui::SameLine();
	ImGui::Text("| 攻撃数: %d", attacks_ ? static_cast<int>(attacks_->size()) : 0);
	ImGui::Separator();

	// 自動リロード設定
	if (ImGui::Checkbox("編集時に自動リロード", &autoReload_))
	{
		Logger(autoReload_
			? "[AttackEditor] 自動リロード: ON\n"
			: "[AttackEditor] 自動リロード: OFF\n");
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::Text("編集のたびに保存してゲーム側に反映します");
		ImGui::EndTooltip();
	}
#endif
}

// ============================================================
// 攻撃リストの描画（左カラム）
// ============================================================
void AttackDataEditor::DrawAttackList()
{
#ifdef USE_IMGUI
	if (!attacks_) { ImGui::Text("攻撃リストがありません。"); return; }

	ImGui::Text("攻撃数 (%d)", static_cast<int>(attacks_->size()));
	ImGui::Separator();

	// ------------------------------------------------------------
	// 攻撃タイプ別にデータを分類
	// ------------------------------------------------------------
	std::map<AttackType, std::vector<int>> byType;
	for (int i = 0; i < static_cast<int>(attacks_->size()); ++i)
		byType[attacks_->at(i).type].push_back(i);

	static const char* typeLabels[] = { "A技 (軽)", "B技 (重)" };

	// ------------------------------------------------------------
	// タイプ別のリスト表示と選択処理
	// ------------------------------------------------------------
	for (int t = 0; t < 2; ++t)
	{
		if (!ImGui::CollapsingHeader(typeLabels[t], ImGuiTreeNodeFlags_DefaultOpen)) continue;

		for (int i : byType[static_cast<AttackType>(t)])
		{
			const bool selected = (i == currentIndex_);
			if (selected)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));

			if (ImGui::Selectable(
				(attacks_->at(i).name + "##atk" + std::to_string(i)).c_str(), selected))
				currentIndex_ = i;

			if (selected)
				ImGui::PopStyleColor();
		}
	}

	ImGui::Separator();

	// ------------------------------------------------------------
	// データ操作用ボタン
	// ------------------------------------------------------------
	if (ImGui::Button("新規")) { NewAttack();       if (autoReload_) TriggerReload(); }
	ImGui::SameLine();
	if (ImGui::Button("複製")) { DuplicateAttack(); if (autoReload_) TriggerReload(); }
	ImGui::SameLine();
	if (ImGui::Button("削除")) { DeleteAttack();    if (autoReload_) TriggerReload(); }

	ImGui::SameLine();
	ImGui::Text(" | ");
	ImGui::SameLine();

	if (ImGui::Button("↑")) { MoveUp();   if (autoReload_) TriggerReload(); }
	ImGui::SameLine();
	if (ImGui::Button("↓")) { MoveDown(); if (autoReload_) TriggerReload(); }
#endif
}

// ============================================================
// 攻撃詳細の描画（右カラム）
// ============================================================
void AttackDataEditor::DrawAttackDetail()
{
#ifdef USE_IMGUI
	if (!attacks_ || currentIndex_ < 0 || currentIndex_ >= static_cast<int>(attacks_->size()))
	{
		ImGui::TextDisabled("攻撃を選択してください");
		return;
	}

	AttackData& atk = attacks_->at(currentIndex_);
	static const char* typeLabels[] = { "A技 (軽)", "B技 (重)", "奥義 (究極)" };
	bool changed = false;

	ImGui::Text("詳細");
	ImGui::Separator();

	// ------------------------------------------------------------
	// 攻撃名称の編集
	// ------------------------------------------------------------
	std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", atk.name.c_str());
	if (ImGui::InputText("名前", nameBuffer_, sizeof(nameBuffer_)))
	{
		atk.name = nameBuffer_; changed = true;
	}
	ImGui::Separator();

	// ------------------------------------------------------------
	// 基本情報の編集
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("基本情報"))
	{
		DrawAnimationSelector(atk, changed);

		int t = static_cast<int>(atk.type);
		if (ImGui::Combo("タイプ", &t, typeLabels, 3))
		{
			atk.type = static_cast<AttackType>(t); changed = true;
		}

		
		// 攻撃エディター側で Player や PlayerCamera へのポインタを持っていると仮定します
		if (player_->GetPlayerCamera()) {
			// 1. 現在有効なカメラ名の一覧を取得
			std::vector<std::string> cameraNames = player_->GetPlayerCamera()->GetAttackCameraNames();

			// 選択肢が空の場合のケア
			if (cameraNames.empty()) {
				cameraNames.push_back("None");
			}

			// 2. 現在の攻撃データに設定されているカメラ名が、リストの何番目にあるか探す
			int currentIdx = -1;
			for (int i = 0; i < static_cast<int>(cameraNames.size()); ++i) {
				if (cameraNames[i] == atk.playCameraWorkName) {
					currentIdx = i;
					break;
				}
			}

			// 未設定の場合は "(未設定)" を表示
			const char* previewLabel = (currentIdx >= 0) ? cameraNames[currentIdx].c_str() : "(未設定)";

			// 3. ImGui のコンボボックスを表示
			if (ImGui::BeginCombo("使用するカメラワーク", previewLabel)) {
				// 未設定に戻す選択肢
				if (ImGui::Selectable("(未設定)", currentIdx < 0)) {
					atk.playCameraWorkName = "";
					changed = true;
				}
				for (int i = 0; i < static_cast<int>(cameraNames.size()); ++i) {
					const bool isSelected = (currentIdx == i);
					if (ImGui::Selectable(cameraNames[i].c_str(), isSelected)) {
						atk.playCameraWorkName = cameraNames[i];
						changed = true;
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}
	}

	// ------------------------------------------------------------
	// 実行タイミング（表示用）
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("タイミング（秒単位・参照用）"))
	{
		changed |= ImGui::InputFloat("持続時間", &atk.duration, 0.01f, 0.1f, "%.2f");
		changed |= ImGui::InputFloat("モーション速度", &atk.motionSpeed, 0.01f, 0.1f, "%.2f");
		changed |= ImGui::InputScalar("ヒット開始フレーム", ImGuiDataType_S32, &atk.hitStart);
		changed |= ImGui::InputScalar("ヒット終了フレーム", ImGuiDataType_S32, &atk.hitEnd);
		changed |= ImGui::InputScalar("先行入力受付開始フレーム", ImGuiDataType_S32, &atk.inputBufferStart);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("このフレーム以降に押された入力をバッファし、同フレーム以降で即時に次の攻撃へ発火する");
		}
		changed |= ImGui::InputScalar("先行入力受付終了フレーム", ImGuiDataType_S32, &atk.comboWindowEnd);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("このフレームを越えるとバッファは破棄される");
		}
	}

	// ------------------------------------------------------------
	// ダメージと物理効果
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("ダメージ & 効果"))
	{
		changed |= ImGui::InputFloat("基本ダメージ", &atk.baseDamage, 1.0f, 10.0f, "%.1f");
		changed |= ImGui::InputFloat("ノックバック", &atk.knockback, 0.1f, 1.0f, "%.1f");
		changed |= ImGui::InputFloat("ノックバック持続時間", &atk.knockbackDuration, 0.1f, 1.0f, "%.2f");
		changed |= ImGui::InputFloat("踏み込み距離", &atk.stepDistance, 0.1f, 1.0f, "%.2f");
	}

	// ------------------------------------------------------------
	// ヒット時の手応え演出（ヒットストップ・画面揺れ）
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("ヒット演出"))
	{
		changed |= ImGui::SliderFloat("ヒットストップ秒数", &atk.hitStopDuration, 0.0f, 0.30f, "%.3f s");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 でヒットストップ無し。0.05〜0.10 が手応え強め");
		changed |= ImGui::SliderFloat("シェイク強度", &atk.shakeIntensity, 0.0f, 1.0f, "%.2f");
		changed |= ImGui::SliderFloat("シェイク継続秒数", &atk.shakeDuration, 0.0f, 0.5f, "%.2f s");
		if (ImGui::SmallButton("軽攻撃プリセット")) {
			atk.hitStopDuration = 0.03f; atk.shakeIntensity = 0.15f; atk.shakeDuration = 0.08f;
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("重攻撃プリセット")) {
			atk.hitStopDuration = 0.06f; atk.shakeIntensity = 0.35f; atk.shakeDuration = 0.12f;
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("演出オフ")) {
			atk.hitStopDuration = 0.0f; atk.shakeIntensity = 0.0f; atk.shakeDuration = 0.0f;
			changed = true;
		}
		changed |= ImGui::DragFloat("コントローラーの振動時間", &atk.inputVibrationTime, 0.0f, 10.0f,0.1f);
		changed |= ImGui::InputScalar("コントローラーの振動強度", ImGuiDataType_U16,&atk.inputVibrationPower);
	}

	// ------------------------------------------------------------
	// オートホーミング（吸い付き）設定
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("オートホーミング"))
	{
		changed |= ImGui::Checkbox("ホーミング有効", &atk.enableHoming);
		changed |= ImGui::SliderFloat("検索距離 (m)", &atk.homingRange, 0.0f, 20.0f, "%.1f");
		changed |= ImGui::SliderFloat("扇半角 (度)", &atk.homingAngleDeg, 0.0f, 180.0f, "%.0f");
		changed |= ImGui::SliderFloat("補正時間 (秒)", &atk.homingDuration, 0.0f, 0.5f, "%.2f");
		changed |= ImGui::SliderFloat("最大前進距離 (m)", &atk.homingMaxStep, 0.0f, 10.0f, "%.2f");
	}

	// ------------------------------------------------------------
	// コンバットコスト（CC）システム
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("CCシステム"))
	{
		changed |= ImGui::InputScalar("CC消費", ImGuiDataType_S32, &atk.ccCost);
		changed |= ImGui::InputScalar("CCヒット時回復", ImGuiDataType_S32, &atk.ccOnHit);
	}

	// ------------------------------------------------------------
	// コンボ接続条件
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("コンボ特性"))
	{
		changed |= ImGui::Checkbox("キャンセル可能", &atk.canCancel);
		changed |= ImGui::Checkbox("任意に連携可能", &atk.canChainToAny);

		if (ImGui::TreeNode("推奨次攻撃"))
		{
			ImGui::TreePop();
		}
	}

	// 更新があれば保存とリロード
	if (changed && autoReload_) { SaveToJson(); TriggerReload(); }
#endif
}

// ============================================================
// プレイヤー用バイナリアニメーション選択
// ============================================================
void AttackDataEditor::DrawAnimationSelector([[maybe_unused]] AttackData& atk, [[maybe_unused]] bool& changed)
{
#ifdef USE_IMGUI
	const auto it = std::find(playerAnimationNames_.begin(), playerAnimationNames_.end(), atk.animationName);
	const bool found = it != playerAnimationNames_.end();
	const char* preview = atk.animationName.empty()
		? "(未設定)"
		: atk.animationName.c_str();

	if (ImGui::BeginCombo("アニメーション", preview))
	{
		if (ImGui::Selectable("(未設定)", atk.animationName.empty()))
		{
			atk.animationName.clear();
			changed = true;
		}

		for (const std::string& name : playerAnimationNames_)
		{
			const bool selected = (atk.animationName == name);
			if (ImGui::Selectable(name.c_str(), selected))
			{
				atk.animationName = name;
				changed = true;
			}
			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::SmallButton("更新##PlayerAnimList"))
	{
		RefreshPlayerAnimationList();
	}

	if (!atk.animationName.empty() && !found)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "未検出: Resources/Binary/Player_%s.anim", atk.animationName.c_str());
	}
#endif
}

// ============================================================
// ドープシート描画（下段カラム）
// ============================================================
void AttackDataEditor::DrawDopeSheet()
{
#ifdef USE_IMGUI
	if (!attacks_ || currentIndex_ < 0 || currentIndex_ >= static_cast<int>(attacks_->size()))
	{
		ImGui::TextDisabled("攻撃を選択するとタイムラインが表示されます");
		return;
	}

	// ------------------------------------------------------------
	// 選択が変更された場合はトラックを再構築
	// ------------------------------------------------------------
	if (currentIndex_ != prevIndex_)
	{
		OnAttackSelected();
		prevIndex_ = currentIndex_;
	}

	AttackData& atk = attacks_->at(currentIndex_);

	// ------------------------------------------------------------
	// タイムラインの全体設定
	// ------------------------------------------------------------
	ImGui::Text("タイムライン : %s", atk.animationName.c_str());
	bool headerChanged = false;
	ImGui::SetNextItemWidth(80.0f);
	headerChanged |= ImGui::InputScalar("総フレーム数", ImGuiDataType_S32, &atk.totalFrames);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(60.0f);
	headerChanged |= ImGui::InputScalar("FPS", ImGuiDataType_S32, &atk.fps);
	atk.totalFrames = std::max(1, atk.totalFrames);
	atk.fps = std::max(1, atk.fps);

	ImGui::Separator();

	// ------------------------------------------------------------
	// ドープシートの更新とデータ書き戻し
	// ------------------------------------------------------------
	bool dopeChanged = dopeEditor_.Draw(
		"##combo_dope",
		dopeTracks_,
		atk.totalFrames,
		atk.fps);

	if (dopeChanged || headerChanged)
	{
		AttackFrameConverter::ApplyTracks(dopeTracks_, atk);
		atk.SyncFramesToSeconds();

		if (autoReload_) { SaveToJson(); TriggerReload(); }
	}
#endif
}

// ============================================================
// 攻撃データ選択時の処理
// フレームデータを基にドープシート用のトラックを作成する
// ============================================================
void AttackDataEditor::OnAttackSelected()
{
	if (!attacks_ || currentIndex_ < 0) return;

	const AttackData& atk = attacks_->at(currentIndex_);

	dopeTracks_ = AttackFrameConverter::BuildTracks(atk);
	dopeEditor_.ResetView();

	Logger(("[AttackEditor] Tracks built for: " + atk.name + "\n").c_str());
}

// ============================================================
// 新規攻撃データの追加
// ============================================================
void AttackDataEditor::NewAttack()
{
	if (!attacks_) return;

	AttackData data;
	data.name = "NewAttack_" + std::to_string(attacks_->size());
	data.animationName = "Idle";
	data.type = AttackType::A_Arte;
	data.playCameraWorkName = "";
	data.totalFrames = 60;
	data.fps = 60;
	data.baseDamage = 30.0f;
	data.hitStart = 0;
	data.hitEnd = 10;
	data.knockback = 5.0f;
	data.knockbackDuration = 0.5f;
	data.ccCost = 1;
	data.canCancel = true;
	data.canChainToAny = true;
	data.motionSpeed = 1.0f;
	data.inputBufferStart = 5;
	data.hitStopDuration = 0.04f;
	data.shakeIntensity = 0.2f;
	data.shakeDuration = 0.10f;
	data.enableHoming = false;
	data.homingRange = 6.0f;
	data.homingAngleDeg = 60.0f;
	data.homingDuration = 0.12f;
	data.homingMaxStep = 3.0f;

	attacks_->push_back(data);
	currentIndex_ = static_cast<int>(attacks_->size()) - 1;
	prevIndex_ = -1;

	Logger("[AttackEditor] New attack created\n");
}

// ============================================================
// 現在の攻撃データの複製
// ============================================================
void AttackDataEditor::DuplicateAttack()
{
	if (!attacks_ || currentIndex_ < 0 || currentIndex_ >= static_cast<int>(attacks_->size())) return;

	AttackData copy = attacks_->at(currentIndex_);
	copy.name += "_copy";
	attacks_->push_back(copy);
	currentIndex_ = static_cast<int>(attacks_->size()) - 1;
	prevIndex_ = -1;

	Logger("[AttackEditor] Attack duplicated\n");
}

// ============================================================
// 攻撃データの削除
// ============================================================
void AttackDataEditor::DeleteAttack()
{
	if (!attacks_ || currentIndex_ < 0 || currentIndex_ >= static_cast<int>(attacks_->size())) return;

	attacks_->erase(attacks_->begin() + currentIndex_);
	if (currentIndex_ >= static_cast<int>(attacks_->size()))
		currentIndex_ = static_cast<int>(attacks_->size()) - 1;

	prevIndex_ = -1;
	dopeTracks_.clear();

	Logger("[AttackEditor] Attack deleted\n");
}

// ============================================================
// リスト上の位置を上へ移動
// ============================================================
void AttackDataEditor::MoveUp()
{
	if (!attacks_ || currentIndex_ <= 0) return;
	std::swap(attacks_->at(currentIndex_), attacks_->at(currentIndex_ - 1));
	--currentIndex_;
}

// ============================================================
// リスト上の位置を下へ移動
// ============================================================
void AttackDataEditor::MoveDown()
{
	if (!attacks_ || currentIndex_ < 0 || currentIndex_ + 1 >= static_cast<int>(attacks_->size())) return;
	std::swap(attacks_->at(currentIndex_), attacks_->at(currentIndex_ + 1));
	++currentIndex_;
}

// ============================================================
// JSONからのロード処理
// ============================================================
void AttackDataEditor::LoadFromJson()
{
	Logger("[AttackEditor] Load start\n");
	if (!attacks_) { Logger("[AttackEditor] ERROR: attacks_ is null\n"); return; }

	if (AttackDatabase::LoadFromFile(filePath_))
	{
		attacks_ = &AttackDatabase::Get();
		currentIndex_ = attacks_->empty() ? -1
			: std::clamp(currentIndex_, 0, static_cast<int>(attacks_->size()) - 1);
		prevIndex_ = -1;
		Logger("[AttackEditor] Load success\n");
	}
	else
	{
		Logger("[AttackEditor] Load failed\n");
	}
}

// ============================================================
// Player_*.anim から攻撃エディタ用のアニメーション名を作る
// ============================================================
std::string AttackDataEditor::ToAnimationNameFromBinaryFile(const std::string& filename)
{
	std::filesystem::path path(filename);
	std::string stem = path.stem().string();

	const std::string prefix = "Player_";
	if (stem.rfind(prefix, 0) == 0)
	{
		stem.erase(0, prefix.size());
	}

	return stem;
}

// ============================================================
// プレイヤー用バイナリアニメーション一覧の更新
// ============================================================
void AttackDataEditor::RefreshPlayerAnimationList()
{
	playerAnimationNames_.clear();

	const std::filesystem::path binaryDir = "Resources/Binary";
	if (!std::filesystem::exists(binaryDir))
	{
		Logger("[AttackEditor] Player animation binary directory not found: Resources/Binary\n");
		return;
	}

	try
	{
		for (const auto& entry : std::filesystem::directory_iterator(binaryDir))
		{
			if (!entry.is_regular_file()) continue;

			const std::filesystem::path path = entry.path();
			if (path.extension() != ".anim") continue;

			const std::string filename = path.filename().string();
			if (filename.rfind("Player_", 0) != 0) continue;

			std::string animationName = ToAnimationNameFromBinaryFile(filename);
			if (animationName.empty()) continue;

			playerAnimationNames_.push_back(animationName);
		}
	}
	catch (const std::exception& e)
	{
		Logger(("[AttackEditor] Failed to scan player animations: " + std::string(e.what()) + "\n").c_str());
		return;
	}

	std::sort(playerAnimationNames_.begin(), playerAnimationNames_.end());
	playerAnimationNames_.erase(
		std::unique(playerAnimationNames_.begin(), playerAnimationNames_.end()),
		playerAnimationNames_.end()
	);

	Logger(("[AttackEditor] Player animations: " + std::to_string(playerAnimationNames_.size()) + "\n").c_str());
}

// ============================================================
// JSONへのセーブ処理
// ============================================================
void AttackDataEditor::SaveToJson()
{
	Logger("[AttackEditor] Save start\n");
	AttackDatabase::SaveToFile(filePath_)
		? Logger("[AttackEditor] Save success\n")
		: Logger("[AttackEditor] Save failed\n");
}

// ============================================================
// データ再読み込みトリガーの実行
// ============================================================
void AttackDataEditor::TriggerReload()
{
	if (onReloadCallback_)
	{
		Logger("[AttackEditor] Reload triggered\n");
		onReloadCallback_();
	}
}
