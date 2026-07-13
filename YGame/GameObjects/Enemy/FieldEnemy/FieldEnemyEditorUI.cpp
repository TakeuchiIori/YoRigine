#include "FieldEnemyEditorUI.h"
#ifdef USE_IMGUI

#include "FieldEnemyManager.h"
#include "Player/Player.h"
#include <Debugger/Logger.h>
#include "imgui.h"
#include <fstream>
#include <json.hpp>
#include <algorithm>

void FieldEnemyEditorUI::ShowDebugInfo(FieldEnemyManager& manager)
{
	ImGui::Text("=== フィールド敵マネージャー ===");
	ImGui::Separator();

	ImGui::Text("アクティブな敵: %d", static_cast<int>(manager.GetActiveEnemyCount()));
	ImGui::Text("スポーンデータ: %d", static_cast<int>(manager.spawnDataMap_.size()));
	ImGui::Text("リスポーンキュー: %d", static_cast<int>(manager.respawnQueue_.size()));
	ImGui::Text("撃破済み敵: %d", static_cast<int>(manager.defeatedEnemyIds_.size()));
	ImGui::Text("エンカウントクールダウン: %.2f秒", manager.encounterCooldown_);
	ImGui::Text("エンカウント発生中: %s", manager.encounterOccurred_ ? "はい" : "いいえ");

	ImGui::Separator();

	ImGui::Checkbox("マネージャー有効", &manager.isActive_);

	if (ImGui::Button("全敵削除")) {
		manager.RemoveAllFieldEnemies();
	}

	ImGui::SameLine();
	if (ImGui::Button("撃破リストクリア")) {
		manager.ClearDefeatedList();
	}

	ImGui::SameLine();
	if (ImGui::Button("全敵エンカウントリセット")) {
		for (auto& enemy : manager.fieldEnemies_) {
			if (enemy) {
				enemy->ResetEncounterState();
			}
		}
		manager.encounterOccurred_ = false;
		manager.encounterCooldown_ = 0.0f;
		Logger("[FieldEnemyManager] 全敵のエンカウント状態をリセット\n");
	}

	ImGui::Separator();
	ImGui::Text("=== 最後のエンカウント ===");
	ImGui::Text("グループ: %s", manager.lastEncounterInfo_.enemyGroup.c_str());
	ImGui::Text("バトルID: %s", manager.lastEncounterInfo_.battleEnemyId.c_str());

	if (manager.lastEncounterInfo_.encounteredEnemy) {
		Vector3 pos = manager.lastEncounterInfo_.encounterPosition;
		ImGui::Text("発生位置: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
	}

	ImGui::Separator();

	// 各敵情報の一覧表示
	if (ImGui::TreeNode("アクティブな敵一覧")) {
		int enemyIndex = 0;
		for (auto& enemy : manager.fieldEnemies_) {
			if (enemy && enemy->IsActive()) {
				const auto& data = enemy->GetEnemyData();
				std::string label = "[" + std::to_string(enemyIndex) + "] " + data.enemyId;

				bool canEncounter = enemy->CanTriggerEncounter();
				if (!canEncounter) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
				}

				if (ImGui::TreeNode(label.c_str())) {
					ImGui::Text("敵ID: %s", data.enemyId.c_str());
					ImGui::Text("バトルID: %s", data.battleEnemyId.c_str());
					ImGui::Text("モデル: %s", data.modelPath.c_str());

					ImGui::Separator();

					Vector3 pos = enemy->GetPosition();
					ImGui::Text("現在位置: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);

					Vector3 spawnPos = enemy->GetSpawnPosition();
					ImGui::Text("スポーン位置: (%.1f, %.1f, %.1f)", spawnPos.x, spawnPos.y, spawnPos.z);

					float distanceFromSpawn = Length(pos - spawnPos);
					ImGui::Text("スポーンからの距離: %.1f", distanceFromSpawn);

					ImGui::Separator();
					const char* stateNames[] = { "巡回", "追跡", "消滅" };
					ImGui::Text("状態: %s", stateNames[static_cast<int>(enemy->GetLogicalState())]);
					ImGui::Text("状態時間: %.2f秒", enemy->GetStateTimer());

					ImGui::Separator();

					ImGui::Text("=== エンカウント情報 ===");
					ImGui::Text("エンカウント済み: %s", enemy->HasTriggeredEncounter() ? "はい" : "いいえ");
					ImGui::Text("エンカウント可能: %s", canEncounter ? "はい" : "いいえ");
					ImGui::Text("クールダウン: %.2f秒", enemy->GetEncounterCooldown());

					if (ImGui::Button("エンカウントリセット")) {
						enemy->ResetEncounterState();
						Logger("[FieldEnemyManager] エンカウントリセット: " + data.enemyId + "\n");
					}
					ImGui::Separator();
					if (ImGui::Button("この敵を削除")) {
						enemy->Despawn();
					}

					ImGui::SameLine();
					if (ImGui::Button("スポーン位置に戻す")) {
						enemy->SetTranslate(spawnPos);
						enemy->ResetStateTimer();
					}

					ImGui::TreePop();
				}

				if (!canEncounter) {
					ImGui::PopStyleColor();
				}

				enemyIndex++;
			}
		}
		ImGui::TreePop();
	}

	ImGui::Separator();

	if (!manager.respawnQueue_.empty() && ImGui::TreeNode("リスポーンキュー")) {
		for (size_t i = 0; i < manager.respawnQueue_.size(); ++i) {
			const auto& respawn = manager.respawnQueue_[i];
			ImGui::Text("[%d] %s - %.1f秒後",
				static_cast<int>(i),
				respawn.spawnData.enemyId.c_str(),
				respawn.timer);
		}
		ImGui::TreePop();
	}

	if (!manager.defeatedEnemyIds_.empty() && ImGui::TreeNode("撃破済み敵")) {
		for (const auto& id : manager.defeatedEnemyIds_) {
			ImGui::Text("- %s", id.c_str());
		}
		ImGui::TreePop();
	}
}

void FieldEnemyEditorUI::ShowEnemyEditor(FieldEnemyManager& manager)
{
	ImGui::Text("=== エネミーエディター ===");
	ImGui::Separator();

	// タブの切り替え
	if (ImGui::BeginTabBar("EnemyEditorTabs")) {

		// 敵データタブ
		if (ImGui::BeginTabItem("敵データ")) {
			ShowEnemyDataEditor(manager);
			ImGui::EndTabItem();
		}

		// スポーンポイントタブ
		if (ImGui::BeginTabItem("スポーンポイント")) {
			ShowSpawnPointEditor(manager);
			ImGui::EndTabItem();
		}

		// プレビューデータタブ
		if (ImGui::BeginTabItem("プレビュー")) {
			ImGui::Text("現在の敵一覧");
			ImGui::Separator();

			for (const auto& pair : manager.enemyDataMap_) {
				const auto& data = pair.second;
				ImGui::Text("ID: %s", data.enemyId.c_str());
				ImGui::Text("  タイプ: %s", data.GetBattleTypeString().c_str());
				ImGui::Text("  モデル: %s", data.modelPath.c_str());

				if (!data.battleEnemyIds.empty()) {
					ImGui::Text("  バトル敵: %d体", static_cast<int>(data.battleEnemyIds.size()));
					for (const auto& id : data.battleEnemyIds) {
						ImGui::Text("    - %s", id.c_str());
					}
				}
				else {
					ImGui::Text("  バトル敵: %s", data.battleEnemyId.c_str());
				}
				ImGui::Separator();
			}
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

/// <summary>
/// 敵データエディターの表示
/// </summary>
void FieldEnemyEditorUI::ShowEnemyDataEditor(FieldEnemyManager& manager)
{
	ImGui::Text("=== 敵データエディター ===");
	ImGui::Separator();

	if (ImGui::Button("新しい敵データを作成", ImVec2(200, 30))) {
		CreateNewEnemyData(manager);
	}

	ImGui::Separator();
	ImGui::Text("既存の敵データ:");

	static char searchBuffer[256] = "";
	ImGui::InputText("検索", searchBuffer, sizeof(searchBuffer));

	ImGui::BeginChild("EnemyDataList", ImVec2(0, 300), true);
	for (auto& pair : manager.enemyDataMap_) {
		const std::string& id = pair.first;
		if (strlen(searchBuffer) > 0 && id.find(searchBuffer) == std::string::npos) continue;

		bool isSelected = (manager.selectedEnemyId_ == id);
		if (ImGui::Selectable(id.c_str(), isSelected)) {
			manager.selectedEnemyId_ = id;
			manager.editorEnemyData_ = pair.second;
		}
	}
	ImGui::EndChild();

	if (!manager.selectedEnemyId_.empty()) {
		ImGui::Separator();
		ImGui::Text("編集中: %s", manager.selectedEnemyId_.c_str());

		// ★ 変更検知フラグ
		bool changed = false;

		ImGui::Text("=== 基本情報 ===");
		static std::string enemyIdBufferOwner;
		static char enemyIdBuffer[128] = {};
		if (enemyIdBufferOwner != manager.selectedEnemyId_) {
			strcpy_s(enemyIdBuffer, manager.editorEnemyData_.enemyId.c_str());
			enemyIdBufferOwner = manager.selectedEnemyId_;
		}
		ImGui::InputText("敵データID", enemyIdBuffer, sizeof(enemyIdBuffer));
		const std::string requestedEnemyId = enemyIdBuffer;
		const bool enemyIdChanged = requestedEnemyId != manager.selectedEnemyId_;
		const bool enemyIdEmpty = requestedEnemyId.empty();
		const bool enemyIdDuplicated = enemyIdChanged && manager.enemyDataMap_.contains(requestedEnemyId);
		if (enemyIdEmpty) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "空IDは不可");
		}
		else if (enemyIdDuplicated) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "同名あり");
		}
		const bool canRenameEnemy = enemyIdChanged && !enemyIdEmpty && !enemyIdDuplicated;
		if (ImGui::Button("敵IDを変更してJSON保存", ImVec2(180, 28))) {
			const std::string oldId = manager.selectedEnemyId_;
			if (canRenameEnemy) {
				manager.editorEnemyData_.enemyId = oldId;
				manager.enemyDataMap_[oldId] = manager.editorEnemyData_;
			}
			if (canRenameEnemy && RenameEnemyData(manager, oldId, requestedEnemyId)) {
				enemyIdBufferOwner.clear();
				manager.SaveEnemyData(FieldEnemyPaths::EnemyData);
				manager.SaveEnemySpawnData(FieldEnemyPaths::Spawn);
				Logger("[EnemyEditor] 敵データIDを変更: " + oldId + " -> " + requestedEnemyId + "\n");
			}
		}
		if (!canRenameEnemy) {
			ImGui::SameLine();
			ImGui::TextDisabled("変更可能なIDを入力してください");
		}

		static char modelPathBuffer[256];
		strcpy_s(modelPathBuffer, manager.editorEnemyData_.modelPath.c_str());
		if (ImGui::InputText("モデルパス", modelPathBuffer, sizeof(modelPathBuffer))) {
			manager.editorEnemyData_.modelPath = modelPathBuffer;
			changed = true;
		}

		const char* battleTypes[] = { "単体", "グループ", "ボス" };
		int currentType = static_cast<int>(manager.editorEnemyData_.battleType);
		if (ImGui::Combo("バトルタイプ", &currentType, battleTypes, 3)) {
			manager.editorEnemyData_.battleType = static_cast<BattleType>(currentType);
			changed = true;
		}

		changed |= ImGui::DragFloat3("スケール", &manager.editorEnemyData_.scale.x, 0.1f, 0.1f, 10.0f);

		ImGui::Separator();
		ImGui::Text("=== バトル設定 ===");
		const std::vector<std::string> battleEnemyOptions = LoadBattleEnemyIdOptions();
		if (manager.editorEnemyData_.battleType == BattleType::Single) {
			const char* preview = manager.editorEnemyData_.battleEnemyId.empty()
				? "(未設定)"
				: manager.editorEnemyData_.battleEnemyId.c_str();
			if (ImGui::BeginCombo("バトル敵ID", preview)) {
				for (const auto& id : battleEnemyOptions) {
					const bool selected = (manager.editorEnemyData_.battleEnemyId == id);
					if (ImGui::Selectable(id.c_str(), selected)) {
						manager.editorEnemyData_.battleEnemyId = id;
						changed = true;
					}
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			if (battleEnemyOptions.empty()) {
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "候補なし");
			}
		}
		else {
			ImGui::Text("バトル敵IDリスト:");
			for (size_t i = 0; i < manager.editorEnemyData_.battleEnemyIds.size(); ++i) {
				ImGui::PushID(static_cast<int>(i));
				ImGui::Text("%d:", static_cast<int>(i + 1));
				ImGui::SameLine();
				const char* preview = manager.editorEnemyData_.battleEnemyIds[i].empty()
					? "(未設定)"
					: manager.editorEnemyData_.battleEnemyIds[i].c_str();
				if (ImGui::BeginCombo("##battleEnemyId", preview)) {
					for (const auto& id : battleEnemyOptions) {
						const bool selected = (manager.editorEnemyData_.battleEnemyIds[i] == id);
						if (ImGui::Selectable(id.c_str(), selected)) {
							manager.editorEnemyData_.battleEnemyIds[i] = id;
							changed = true;
						}
						if (selected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				ImGui::SameLine();
				if (ImGui::Button("削除")) {
					manager.editorEnemyData_.battleEnemyIds.erase(manager.editorEnemyData_.battleEnemyIds.begin() + i);
					changed = true;
				}
				ImGui::PopID();
			}

			static int addBattleEnemyIndex = 0;
			if (addBattleEnemyIndex >= static_cast<int>(battleEnemyOptions.size())) {
				addBattleEnemyIndex = 0;
			}
			const char* addPreview = battleEnemyOptions.empty()
				? "(候補なし)"
				: battleEnemyOptions[addBattleEnemyIndex].c_str();
			if (ImGui::BeginCombo("追加するバトル敵ID", addPreview)) {
				for (int i = 0; i < static_cast<int>(battleEnemyOptions.size()); ++i) {
					const bool selected = (addBattleEnemyIndex == i);
					if (ImGui::Selectable(battleEnemyOptions[i].c_str(), selected)) {
						addBattleEnemyIndex = i;
					}
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (ImGui::Button("追加") && !battleEnemyOptions.empty()) {
				manager.editorEnemyData_.battleEnemyIds.push_back(battleEnemyOptions[addBattleEnemyIndex]);
				changed = true;
			}
		}

		const char* formationValues[] = { "default", "single", "dual", "triple", "quad" };
		const char* formationLabels[] = {
			"default (人数で自動)",
			"single (1体)",
			"dual (2体)",
			"triple (3体)",
			"quad (4体)"
		};
		constexpr int formationCount = static_cast<int>(sizeof(formationValues) / sizeof(formationValues[0]));
		int formationIndex = 0;
		for (int i = 0; i < formationCount; ++i) {
			if (manager.editorEnemyData_.battleFormation == formationValues[i]) {
				formationIndex = i;
				break;
			}
		}
		const char* preview = manager.editorEnemyData_.battleFormation.empty()
			? formationLabels[0]
			: formationLabels[formationIndex];
		if (ImGui::BeginCombo("バトルフォーメーション", preview)) {
			for (int i = 0; i < formationCount; ++i) {
				const bool selected = (formationIndex == i);
				if (ImGui::Selectable(formationLabels[i], selected)) {
					manager.editorEnemyData_.battleFormation = formationValues[i];
					changed = true;
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();
		ImGui::Text("=== 移動パラメータ ===");
		changed |= ImGui::DragFloat("巡回半径", &manager.editorEnemyData_.patrolRadius, 0.5f, 0.0f, 50.0f);
		changed |= ImGui::DragFloat("巡回速度", &manager.editorEnemyData_.patrolSpeed, 0.1f, 0.1f, 20.0f);
		changed |= ImGui::DragFloat("追跡速度", &manager.editorEnemyData_.chaseSpeed, 0.1f, 0.1f, 20.0f);
		changed |= ImGui::DragFloat("追跡範囲", &manager.editorEnemyData_.chaseRange, 0.5f, 1.0f, 50.0f);
		changed |= ImGui::DragFloat("帰還距離", &manager.editorEnemyData_.returnDistance, 0.5f, 1.0f, 50.0f);

		ImGui::Separator();
		ImGui::Text("=== 視界・索敵パラメータ ===");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("DrawViewCone の描画と完全に連動します");
		}
		changed |= ImGui::DragFloat("視界距離", &manager.editorEnemyData_.viewDistance, 0.5f, 1.0f, 100.0f);
		changed |= ImGui::DragFloat("視野角 (度)", &manager.editorEnemyData_.viewAngle, 1.0f, 1.0f, 180.0f);
		changed |= ImGui::DragFloat("足音検知範囲", &manager.editorEnemyData_.noiseDetectionRange, 0.5f, 1.0f, 50.0f);

		ImGui::Separator();
		ImGui::Text("=== 回転・リアクションパラメータ ===");
		ImGui::SetNextItemWidth(200.0f);
		changed |= ImGui::DragFloat("補間回転速度 (rad/s)", &manager.editorEnemyData_.rotationSpeed, 0.1f, 0.5f, 30.0f);
		ImGui::SameLine();
		ImGui::TextDisabled("(大きいほど素早く向く)");

		changed |= ImGui::DragFloat("発見リアクション時間 (秒)", &manager.editorEnemyData_.alertDuration, 0.05f, 0.1f, 3.0f);
		ImGui::SameLine();
		ImGui::TextDisabled("！停止時間");

		changed |= ImGui::DragFloat("索敵継続時間 (秒)", &manager.editorEnemyData_.searchDuration, 0.1f, 0.5f, 10.0f);
		ImGui::SameLine();
		ImGui::TextDisabled("？首振り時間");

		changed |= ImGui::DragFloat("索敵スウィープ角 (度)", &manager.editorEnemyData_.searchSweepAngle, 1.0f, 10.0f, 120.0f);
		ImGui::SameLine();
		ImGui::TextDisabled("左右に振れる角度");

		ImGui::Separator();
		ImGui::Text("=== 見た目設定 ===");
		changed |= ImGui::Checkbox("カスタムカラーを使用", &manager.editorEnemyData_.useCustomColor);
		if (manager.editorEnemyData_.useCustomColor) {
			changed |= ImGui::ColorEdit4("モデルカラー", &manager.editorEnemyData_.modelColor.x);
		}

		ImGui::Separator();
		ImGui::Text("=== スポットライト(視界)設定 ===");
		changed |= ImGui::Checkbox("スポットライトを使用", &manager.editorEnemyData_.useSpotLight);
		if (manager.editorEnemyData_.useSpotLight) {
			changed |= ImGui::ColorEdit4("ライトカラー", &manager.editorEnemyData_.spotLightColor.x);
			changed |= ImGui::DragFloat("強度", &manager.editorEnemyData_.spotLightIntensity, 0.1f, 0.0f, 1000.0f);
			changed |= ImGui::DragFloat("減衰(Decay)", &manager.editorEnemyData_.spotLightDecay, 0.1f, 0.0f, 10.0f);
			changed |= ImGui::DragFloat3("位置オフセット", &manager.editorEnemyData_.spotLightOffset.x, 0.1f);
			changed |= ImGui::DragFloat("ピッチ(下向き加減)", &manager.editorEnemyData_.spotLightPitch, 0.01f, -1.0f, 1.0f);
		}


		// ★★★ リアルタイム反映処理 ★★★
		if (changed) {
			// 1. マスターデータを更新
			manager.enemyDataMap_[manager.selectedEnemyId_] = manager.editorEnemyData_;

			// 2. 現在出現中の同じIDを持つ敵すべてに反映
			for (auto& enemy : manager.fieldEnemies_) {
				if (enemy && enemy->GetEnemyData().enemyId == manager.selectedEnemyId_) {
					enemy->ApplyUpdatedData(manager.editorEnemyData_);
				}
			}
		}

		ImGui::Separator();
		if (ImGui::Button("変更を保存(JSON)", ImVec2(120, 30))) {
			if (canRenameEnemy) {
				const std::string oldId = manager.selectedEnemyId_;
				manager.editorEnemyData_.enemyId = oldId;
				manager.enemyDataMap_[oldId] = manager.editorEnemyData_;
				if (RenameEnemyData(manager, oldId, requestedEnemyId)) {
					enemyIdBufferOwner.clear();
					Logger("[EnemyEditor] 敵データIDを変更: " + oldId + " -> " + requestedEnemyId + "\n");
				}
			}
			else {
				manager.editorEnemyData_.enemyId = manager.selectedEnemyId_;
				manager.enemyDataMap_[manager.selectedEnemyId_] = manager.editorEnemyData_;
			}
			manager.SaveEnemyData(FieldEnemyPaths::EnemyData);
			manager.SaveEnemySpawnData(FieldEnemyPaths::Spawn);
			Logger("[EnemyEditor] 敵データをファイルに保存しました\n");
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル", ImVec2(120, 30))) {
			manager.editorEnemyData_ = manager.enemyDataMap_[manager.selectedEnemyId_];
			// キャンセル時も念のため再同期して戻す
			for (auto& enemy : manager.fieldEnemies_) {
				if (enemy && enemy->GetEnemyData().enemyId == manager.selectedEnemyId_) {
					enemy->ApplyUpdatedData(manager.editorEnemyData_);
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("削除", ImVec2(120, 30))) {
			DeleteEnemyData(manager, manager.selectedEnemyId_);
		}
	}
}

/// <summary>
/// スポーンポイントエディターの表示
/// </summary>
void FieldEnemyEditorUI::ShowSpawnPointEditor(FieldEnemyManager& manager)
{
	ImGui::Text("=== スポーンポイントエディター ===");
	ImGui::Separator();

	if (ImGui::Button("新しいスポーンポイントを作成", ImVec2(220, 30))) {
		CreateNewSpawnPoint(manager);
	}

	ImGui::SameLine();
	if (ImGui::Button("全スポーンを実行", ImVec2(140, 30))) {
		manager.SpawnAllPending();
	}
	ImGui::SameLine();
	if (ImGui::Button("全敵をデスポーン", ImVec2(140, 30))) {
		manager.DespawnAll();
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("敵を隠してスポーン点だけ表示", &manager.editorHideEnemies_)) {
		// 切替時にコライダーも有効/無効を揃える (バトル中の挙動と同じ理由で必要)
		for (auto& enemy : manager.fieldEnemies_) {
			if (enemy) enemy->SetCollisionActive(!manager.editorHideEnemies_);
		}
	}

	ImGui::Separator();
	ImGui::Text("スポーンポイント一覧:");

	ImGui::BeginChild("SpawnPointList", ImVec2(0, 250), true);
	for (auto& pair : manager.spawnDataMap_) {
		const std::string& id = pair.first;
		const auto& spawn = pair.second;
		bool isSelected = (manager.selectedSpawnId_ == id);

		std::string label = id + " (" + spawn.enemyId + ")";
		if (ImGui::Selectable(label.c_str(), isSelected)) {
			manager.selectedSpawnId_ = id;
			manager.editorSpawnData_ = spawn;
		}
	}
	ImGui::EndChild();

	if (!manager.selectedSpawnId_.empty()) {
		ImGui::Separator();
		ImGui::Text("編集中: %s", manager.selectedSpawnId_.c_str());
		ImGui::Text("=== 基本設定 ===");

		static std::string idBufferOwner;
		static char idBuffer[128] = {};
		if (idBufferOwner != manager.selectedSpawnId_) {
			strcpy_s(idBuffer, manager.editorSpawnData_.id.c_str());
			idBufferOwner = manager.selectedSpawnId_;
		}
		ImGui::InputText("スポーンID", idBuffer, sizeof(idBuffer));
		const std::string requestedId = idBuffer;
		const bool idChanged = requestedId != manager.selectedSpawnId_;
		const bool idEmpty = requestedId.empty();
		const bool idDuplicated = idChanged && manager.spawnDataMap_.contains(requestedId);
		if (idEmpty) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "空IDは不可");
		}
		else if (idDuplicated) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "同名あり");
		}
		const bool canRename = idChanged && !idEmpty && !idDuplicated;
		if (ImGui::Button("IDを変更してJSON保存", ImVec2(170, 28))) {
			const std::string oldId = manager.selectedSpawnId_;
			if (canRename) {
				manager.editorSpawnData_.id = oldId;
				manager.spawnDataMap_[oldId] = manager.editorSpawnData_;
			}
			if (canRename && RenameSpawnPoint(manager, oldId, requestedId)) {
				idBufferOwner.clear();
				manager.SaveEnemySpawnData(FieldEnemyPaths::Spawn);
				Logger("[EnemyEditor] スポーンポイントIDを変更: " + oldId + " -> " + requestedId + "\n");
			}
		}
		if (!canRename) {
			ImGui::SameLine();
			ImGui::TextDisabled("変更可能なIDを入力してください");
		}

		if (ImGui::BeginCombo("敵ID", manager.editorSpawnData_.enemyId.c_str())) {
			for (const auto& pair : manager.enemyDataMap_) {
				bool isSelected = (pair.first == manager.editorSpawnData_.enemyId);
				if (ImGui::Selectable(pair.first.c_str(), isSelected)) {
					manager.editorSpawnData_.enemyId = pair.first;
				}
			}
			ImGui::EndCombo();
		}

		bool posChanged = ImGui::DragFloat3("位置", &manager.editorSpawnData_.position.x, 0.5f);
		if (manager.player_ && ImGui::Button("プレイヤーの位置に配置")) {
			manager.editorSpawnData_.position = manager.player_->GetWorldPosition();
			posChanged = true;
		}
		// 位置編集はマーカー/ギズモが見える位置にあるべきなので即座にライブ反映
		// (変更を保存ボタンは respawn / その他フィールドの永続化のために残す)
		if (posChanged) {
			auto it = manager.spawnDataMap_.find(manager.selectedSpawnId_);
			if (it != manager.spawnDataMap_.end()) {
				it->second.position = manager.editorSpawnData_.position;
			}
		}

		ImGui::Separator();
		ImGui::Text("=== スポーン制御 ===");
		ImGui::Checkbox("起動時にスポーン", &manager.editorSpawnData_.spawnOnLoad);
		if (!manager.editorSpawnData_.spawnOnLoad) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), " (バッチスポーン待機)");
		}

		ImGui::Separator();
		ImGui::Text("=== リスポーン設定 ===");
		ImGui::Checkbox("アクティブ", &manager.editorSpawnData_.isActive);
		ImGui::Checkbox("バトル後にリスポーン", &manager.editorSpawnData_.respawnAfterBattle);
		if (manager.editorSpawnData_.respawnAfterBattle) {
			ImGui::DragFloat("リスポーン遅延(秒)", &manager.editorSpawnData_.respawnDelay, 1.0f, 0.0f, 300.0f);
		}

		static char conditionBuffer[256];
		strcpy_s(conditionBuffer, manager.editorSpawnData_.spawnCondition.c_str());
		if (ImGui::InputText("スポーン条件", conditionBuffer, sizeof(conditionBuffer))) {
			manager.editorSpawnData_.spawnCondition = conditionBuffer;
		}

		static char commentBuffer[512];
		strcpy_s(commentBuffer, manager.editorSpawnData_.comment.c_str());
		if (ImGui::InputTextMultiline("コメント", commentBuffer, sizeof(commentBuffer))) {
			manager.editorSpawnData_.comment = commentBuffer;
		}

		ImGui::Separator();
		ImGui::Checkbox("エディター専用", &manager.editorSpawnData_.isEditorOnly);
		if (manager.editorSpawnData_.isEditorOnly) {
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "※ゲーム実行時には表示されません");
		}

		ImGui::Separator();
		if (ImGui::Button("変更を保存", ImVec2(120, 30))) {
			if (canRename) {
				const std::string oldId = manager.selectedSpawnId_;
				manager.editorSpawnData_.id = oldId;
				manager.spawnDataMap_[oldId] = manager.editorSpawnData_;
				if (RenameSpawnPoint(manager, oldId, requestedId)) {
					idBufferOwner.clear();
					Logger("[EnemyEditor] スポーンポイントIDを変更: " + oldId + " -> " + requestedId + "\n");
				}
			}
			else {
				manager.editorSpawnData_.id = manager.selectedSpawnId_;
			}
			manager.spawnDataMap_[manager.selectedSpawnId_] = manager.editorSpawnData_;
			manager.SaveEnemySpawnData(FieldEnemyPaths::Spawn);
			Logger("[EnemyEditor] スポーンポイントを保存: " + manager.selectedSpawnId_ + "\n");
		}

		ImGui::SameLine();
		if (ImGui::Button("即座にスポーン", ImVec2(120, 30))) {
			manager.SpawnFieldEnemy(manager.editorSpawnData_);
		}

		ImGui::SameLine();
		if (ImGui::Button("削除", ImVec2(120, 30))) {
			DeleteSpawnPoint(manager, manager.selectedSpawnId_);
		}
	}
}

/// <summary>
/// 新しい敵データを作成
/// </summary>
void FieldEnemyEditorUI::CreateNewEnemyData(FieldEnemyManager& manager)
{
	std::string newId = GenerateUniqueEnemyDataId(manager, "NewEnemy");

	FieldEnemyData newData;
	newData.enemyId = newId;
	newData.modelPath = "default_enemy.obj";
	newData.battleEnemyId = "alien";
	newData.battleType = BattleType::Single;

	manager.enemyDataMap_[newId] = newData;
	manager.selectedEnemyId_ = newId;
	manager.editorEnemyData_ = newData;

	Logger("[EnemyEditor] 新しい敵データを作成: " + newId + "\n");
}

std::string FieldEnemyEditorUI::GenerateUniqueEnemyDataId(FieldEnemyManager& manager, const std::string& prefix)
{
	const std::string base = prefix.empty() ? "NewEnemy" : prefix;
	for (int i = 1; i < 100000; ++i) {
		const std::string candidate = base + "_" + std::to_string(i);
		if (!manager.enemyDataMap_.contains(candidate)) {
			return candidate;
		}
	}

	return base + "_overflow";
}

bool FieldEnemyEditorUI::RenameEnemyData(FieldEnemyManager& manager, const std::string& oldId, const std::string& newId)
{
	if (oldId.empty() || newId.empty() || oldId == newId) return false;
	if (manager.enemyDataMap_.contains(newId)) return false;

	auto it = manager.enemyDataMap_.find(oldId);
	if (it == manager.enemyDataMap_.end()) return false;

	FieldEnemyData renamed = it->second;
	renamed.enemyId = newId;
	manager.enemyDataMap_.erase(it);
	manager.enemyDataMap_[newId] = renamed;

	for (auto& [spawnId, spawn] : manager.spawnDataMap_) {
		if (spawn.enemyId == oldId) {
			spawn.enemyId = newId;
		}
	}

	for (auto& respawn : manager.respawnQueue_) {
		if (respawn.spawnData.enemyId == oldId) {
			respawn.spawnData.enemyId = newId;
		}
	}

	for (auto& enemy : manager.fieldEnemies_) {
		if (enemy && enemy->GetEnemyData().enemyId == oldId) {
			enemy->ApplyUpdatedData(renamed);
		}
	}

	manager.selectedEnemyId_ = newId;
	manager.editorEnemyData_ = renamed;
	return true;
}

std::vector<std::string> FieldEnemyEditorUI::LoadBattleEnemyIdOptions()
{
	std::vector<std::string> ids;

	try {
		std::ifstream file("Resources/Json/BattleEnemies/enemy_data.json");
		if (!file.is_open()) {
			return ids;
		}

		nlohmann::json json;
		file >> json;

		if (!json.contains("battleEnemies") || !json["battleEnemies"].is_array()) {
			return ids;
		}

		for (const auto& enemyJson : json["battleEnemies"]) {
			std::string id = enemyJson.value("enemyId", std::string(""));
			if (!id.empty() && std::find(ids.begin(), ids.end(), id) == ids.end()) {
				ids.push_back(id);
			}
		}
	}
	catch (const std::exception& e) {
		Logger("[EnemyEditor] バトル敵ID候補の読み込み失敗: " + std::string(e.what()) + "\n");
	}

	return ids;
}

/// <summary>
/// 指定された敵データを削除
/// </summary>
void FieldEnemyEditorUI::DeleteEnemyData(FieldEnemyManager& manager, const std::string& enemyId)
{
	manager.enemyDataMap_.erase(enemyId);
	manager.selectedEnemyId_.clear();

	manager.SaveEnemyData(FieldEnemyPaths::EnemyData);
	Logger("[EnemyEditor] 敵データを削除: " + enemyId + "\n");
}

/// <summary>
/// 新しいスポーンポイントを作成
/// </summary>
void FieldEnemyEditorUI::CreateNewSpawnPoint(FieldEnemyManager& manager)
{
	std::string newId = manager.GenerateUniqueSpawnPointId("Spawn");

	FieldEnemySpawnData newSpawn;
	newSpawn.id = newId;
	newSpawn.enemyId = manager.enemyDataMap_.empty() ? "alien" : manager.enemyDataMap_.begin()->first;
	// 既定位置はプレイヤーの足元。原点 (0,0,0) だと地面に埋まる + 遠くて目視できないため。
	newSpawn.position = manager.player_ ? manager.player_->GetWorldPosition() : Vector3(0.0f, 0.0f, 0.0f);
	newSpawn.isActive = true;

	manager.spawnDataMap_[newId] = newSpawn;
	manager.selectedSpawnId_ = newId;
	manager.editorSpawnData_ = newSpawn;

	Logger("[EnemyEditor] 新しいスポーンポイントを作成: " + newId + "\n");
}

bool FieldEnemyEditorUI::RenameSpawnPoint(FieldEnemyManager& manager, const std::string& oldId, const std::string& newId)
{
	if (oldId.empty() || newId.empty() || oldId == newId) return false;
	if (manager.spawnDataMap_.contains(newId)) return false;

	auto it = manager.spawnDataMap_.find(oldId);
	if (it == manager.spawnDataMap_.end()) return false;

	FieldEnemySpawnData renamed = it->second;
	renamed.id = newId;
	manager.spawnDataMap_.erase(it);
	manager.spawnDataMap_[newId] = renamed;

	for (auto& enemy : manager.fieldEnemies_) {
		if (enemy && enemy->GetSpawnId() == oldId) {
			enemy->SetSpawnId(newId);
		}
	}

	for (auto& respawn : manager.respawnQueue_) {
		if (respawn.spawnData.id == oldId) {
			respawn.spawnData.id = newId;
		}
	}

	manager.selectedSpawnId_ = newId;
	manager.editorSpawnData_ = renamed;
	return true;
}

/// <summary>
/// 指定されたスポーンポイントを削除
/// </summary>
void FieldEnemyEditorUI::DeleteSpawnPoint(FieldEnemyManager& manager, const std::string& spawnId)
{
	// 実際にスポーンされている敵も削除
	manager.RemoveFieldEnemy(spawnId);

	manager.spawnDataMap_.erase(spawnId);
	manager.selectedSpawnId_.clear();

	manager.SaveEnemySpawnData(FieldEnemyPaths::Spawn);
	Logger("[EnemyEditor] スポーンポイントを削除: " + spawnId + "\n");
}

#endif
