#include "BattleEnemyEditorUI.h"
#ifdef USE_IMGUI

#include "BattleEnemyManager.h"
#include "BattleEnemy.h"
#include <Debugger/Logger.h>
#include "imgui.h"
#include <utility>

namespace {

/// <summary>
/// 間合い取りパラメータの編集UI。
/// ベースデータ編集と個体編集の両方から呼ぶので関数に切り出している。
/// </summary>
void DrawSpacingEditor(SpacingParams& sp, const char* idSuffix)
{
	ImGui::PushID(idSuffix);

	ImGui::SeparatorText("共通");
	ImGui::DragFloat("維持したい間合い", &sp.preferredDistance, 0.1f, 1.0f, 30.0f, "%.1fm");
	ImGui::DragFloat("近すぎ判定距離", &sp.tooCloseDistance, 0.1f, 0.5f, 20.0f, "%.1fm");
	ImGui::DragFloat("プレイヤーへ向く速度", &sp.faceRotationSpeed, 0.1f, 0.1f, 30.0f, "%.1frad/s");

	ImGui::SeparatorText("後退 (Backstep)");
	ImGui::DragFloat("後退時間", &sp.backstepDuration, 0.05f, 0.05f, 3.0f, "%.2f秒");
	ImGui::DragFloat("後退速度倍率", &sp.backstepSpeedMultiplier, 0.1f, 0.1f, 10.0f, "x%.1f");

	ImGui::SeparatorText("横移動 (Strafe)");
	ImGui::DragFloat("最短時間##strafe", &sp.strafeMinDuration, 0.05f, 0.05f, 5.0f, "%.2f秒");
	ImGui::DragFloat("最長時間##strafe", &sp.strafeMaxDuration, 0.05f, 0.05f, 8.0f, "%.2f秒");
	ImGui::DragFloat("横移動速度倍率", &sp.strafeSpeedMultiplier, 0.05f, 0.0f, 5.0f, "x%.2f");
	ImGui::DragFloat("間合い維持の強さ", &sp.strafeDistanceKeepStrength, 0.05f, 0.0f, 10.0f, "%.2f");

	ImGui::SeparatorText("様子見 (Observe)");
	ImGui::DragFloat("最短時間##observe", &sp.observeMinDuration, 0.05f, 0.0f, 5.0f, "%.2f秒");
	ImGui::DragFloat("最長時間##observe", &sp.observeMaxDuration, 0.05f, 0.0f, 8.0f, "%.2f秒");

	ImGui::SeparatorText("攻撃後の選択重み");
	ImGui::DragFloat("後退##weight", &sp.backstepWeight, 0.05f, 0.0f, 10.0f, "%.2f");
	ImGui::DragFloat("横移動##weight", &sp.strafeWeight, 0.05f, 0.0f, 10.0f, "%.2f");
	ImGui::DragFloat("様子見##weight", &sp.observeWeight, 0.05f, 0.0f, 10.0f, "%.2f");

	// 最短 > 最長 になると乱数レンジが壊れるので、その場で入れ替えておく
	if (sp.strafeMaxDuration < sp.strafeMinDuration)   std::swap(sp.strafeMinDuration, sp.strafeMaxDuration);
	if (sp.observeMaxDuration < sp.observeMinDuration) std::swap(sp.observeMinDuration, sp.observeMaxDuration);

	ImGui::PopID();
}

} // namespace

void BattleEnemyEditorUI::Draw(BattleEnemyManager& manager)
{
	if (ImGui::Button("敵データ読み込み")) {
		manager.LoadEnemyData(manager.enemyDataFilePath_);
	}
	ImGui::Text("戦闘中: %s", manager.isBattleActive_ ? "はい" : "いいえ");
	ImGui::Text("一時停止: %s", manager.isBattlePaused_ ? "はい" : "いいえ");
	ImGui::Text("アクティブな敵: %zu", manager.GetActiveEnemyCount());
	ImGui::Text("戦闘時間: %.1f秒", manager.battleTimer_);
	ImGui::Text("現在のエンカウント: %s", manager.currentEncounterName_.c_str());

	const char* resultStrings[] = { "なし", "勝利", "敗北", "逃走", "進行中" };
	ImGui::Text("戦闘結果: %s", resultStrings[static_cast<int>(manager.battleResult_)]);

	ImGui::Separator();

	ImGui::Text("=== 戦闘統計 ===");
	ImGui::Text("撃破数: %d", manager.battleStats_.enemiesDefeated);
	ImGui::Text("戦闘時間: %.1f秒", manager.battleStats_.battleDuration);

	ImGui::Separator();

	ImGui::SameLine();
	if (ImGui::Button("戦闘終了")) {
		manager.ForceBattleEnd();
	}

	if (manager.isBattleActive_) {
		if (ImGui::Button("一時停止/再開")) {
			manager.PauseBattle(!manager.isBattlePaused_);
		}

		ImGui::SameLine();
		if (ImGui::Button("勝利")) {
			manager.EndBattle(BattleResult::Victory);
		}
		ImGui::SameLine();
		if (ImGui::Button("敗北")) {
			manager.EndBattle(BattleResult::Defeat);
		}
	}

	ImGui::Separator();

	if (ImGui::Button("全敵スタン(2秒)")) {
		manager.StunAllEnemies(2.0f);
	}
	ImGui::SameLine();
	if (ImGui::Button("全敵ダメージ(50)")) {
		manager.DamageAllEnemies(50);
	}

	ImGui::Separator();

	static char enemyIdBuffer[256] = "goblin";
	static float spawnPos[3] = { 0.0f, 0.0f, 5.0f };

	ImGui::InputText("敵ID", enemyIdBuffer, sizeof(enemyIdBuffer));
	ImGui::InputFloat3("生成位置", spawnPos);

	if (ImGui::Button("デバッグ生成")) {
		Vector3 position(spawnPos[0], spawnPos[1], spawnPos[2]);
		manager.DebugSpawnEnemy(position, enemyIdBuffer);
	}

	ImGui::Separator();

	// ★敵ベースデータ編集（攻撃パターン編集機能追加）★
	if (ImGui::TreeNode("敵ベースデータ編集 （これを調整するとその敵全部に反映）")) {

		// 使用可能な攻撃パターンのリスト ("sidestep" は対応する攻撃状態が存在しないため削除)
		static const AttackPatternType availablePatterns[] = {
			AttackPatternType::Rush, AttackPatternType::Jump, AttackPatternType::Spin,
			AttackPatternType::ChargeRush, AttackPatternType::Combo
		};
		static const char* availablePatternNames[] = {
			"rush", "jump", "spin", "chargeRush", "combo"
		};
		static const int patternCount = 5;

		for (auto& pair : manager.enemyDataMap_) {
			BattleEnemyData& data = pair.second;

			if (ImGui::TreeNode(data.enemyId.c_str())) {

				ImGui::DragInt("HP", &data.hp, 1, 1, 9999);
				ImGui::DragInt("攻撃力", &data.attack, 1, 1, 999);
				ImGui::DragInt("防御力", &data.defense, 1, 1, 999);
				ImGui::DragFloat("移動速度", &data.moveSpeed, 0.1f, 0.1f, 50.0f);
				ImGui::DragFloat("追跡開始距離", &data.approachStateRange, 0.5f, 1.0f, 100.0f);
				ImGui::DragFloat("攻撃開始距離", &data.attackStateRange, 0.5f, 1.0f, 50.0f);

				// --- 間合い取り（攻撃と攻撃の「間」）---
				// 戦っている感じはここの数値で決まるので、攻撃詳細より前に置いている。
				if (ImGui::CollapsingHeader("間合い取り (攻撃の合間の動き)")) {
					DrawSpacingEditor(data.spacing, "base");
				}

				// --- 攻撃詳細パラメータ ---
				if (ImGui::CollapsingHeader("攻撃詳細設定 (各Stateの数値)")) {

					// --- Rush ---
					if (ImGui::TreeNode("Rush (基本突進)")) {
						auto& r = data.attackParams.rush;
						ImGui::DragFloat("予備動作", &r.anticipationTime, 0.05f, 0.0f, 3.0f, "%.2f秒");
						ImGui::DragFloat(" 後退する距離", &r.anticipationDistance, 0.05f, 0.0f, 5.0f, "%.2秒");
						ImGui::DragFloat("溜め時間", &r.chargeTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::DragFloat("突進時間", &r.rushTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::DragFloat("速度倍率", &r.speedMultiplier, 0.1f, 0.0f, 20.0f, "x%.1f");
						ImGui::DragFloat("後隙", &r.cooldownTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::TreePop();
					}

					// --- chargeRush ---
					if (ImGui::TreeNode("chargeRush (強力突進)")) {
						auto& c = data.attackParams.chargeRush;
						ImGui::DragFloat("溜め(追尾)時間", &c.chargeTime, 0.05f, 0.0f, 10.0f, "%.2f秒");
						ImGui::DragFloat("突進時間", &c.rushTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::DragFloat("速度倍率", &c.speedMultiplier, 0.1f, 0.0f, 30.0f, "x%.1f");
						ImGui::DragFloat("後隙", &c.cooldownTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::TreePop();
					}

					// --- Spin ---
					if (ImGui::TreeNode("Spin (回転攻撃)")) {
						auto& s = data.attackParams.spin;
						ImGui::DragFloat("予備動作", &s.chargeTime, 0.05f, 0.0f, 3.0f, "%.2f秒");
						ImGui::DragFloat("回転時間", &s.spinTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::DragFloat("回転数", &s.rotationCount, 0.1f, 0.0f, 10.0f, "%.1f回");
						ImGui::DragFloat("移動倍率", &s.moveSpeedMultiplier, 0.1f, 0.0f, 10.0f, "x%.1f");
						ImGui::DragFloat("後隙", &s.cooldownTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::TreePop();
					}

					// --- jump ---
					if (ImGui::TreeNode("jump (ジャンプ攻撃)")) {
						auto& l = data.attackParams.jump;
						ImGui::DragFloat("踏み込み時間", &l.chargeTime, 0.05f, 0.0f, 3.0f, "%.2f秒");
						ImGui::DragFloat("滞空時間", &l.jumpTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::DragFloat("ジャンプ高度", &l.jumpHeight, 0.1f, 0.0f, 20.0f, "%.1fm");
						ImGui::DragFloat("しゃがみ深さ", &l.crouchDepth, 0.05f, 0.0f, 2.0f, "%.2fm");
						ImGui::DragFloat("後隙", &l.cooldownTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::TreePop();
					}

					// --- Combo ---
					if (ImGui::TreeNode("Combo (3連撃)")) {
						auto& cb = data.attackParams.combo;
						ImGui::DragFloat("1段の時間", &cb.phaseDuration, 0.05f, 0.1f, 5.0f, "%.2f秒");
						ImGui::DragFloat("段内溜め", &cb.subChargeTime, 0.05f, 0.0f, 2.0f, "%.2f秒");
						ImGui::DragFloat("段内突進", &cb.subRushTime, 0.05f, 0.0f, 2.0f, "%.2f秒");
						ImGui::DragFloat("加速倍率", &cb.rushSpeedMultiplier, 0.1f, 0.0f, 20.0f, "x%.1f");
						ImGui::DragFloat("全体後隙", &cb.cooldownTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::TreePop();
					}

					// --- Counter ---
					if (ImGui::TreeNode("Counter (反撃)")) {
						auto& ct = data.attackParams.counter;
						ImGui::Checkbox("反撃有効", &ct.enabled);
						ImGui::Separator();
						ImGui::TextDisabled("トリガー条件");
						ImGui::DragInt(" 連続被弾しきい値", &ct.triggerHitCount, 1, 1, 20);
						ImGui::DragFloat(" カウントリセット秒数", &ct.hitCountResetTime, 0.1f, 0.1f, 10.0f, "%.1f秒");
						ImGui::Separator();
						ImGui::TextDisabled("Recovery（気合溜め・無敵）");
						ImGui::DragFloat(" 回復時間", &ct.recoveryDuration, 0.05f, 0.1f, 5.0f, "%.2f秒");
						ImGui::Separator();
						ImGui::TextDisabled("CounterAttack 内部フェーズ");
						ImGui::DragFloat(" 起動時間", &ct.startupTime, 0.05f, 0.0f, 2.0f, "%.2f秒");
						ImGui::DragFloat(" 後退時間", &ct.anticipationTime, 0.05f, 0.0f, 3.0f, "%.2f秒");
						ImGui::DragFloat(" 後退距離", &ct.anticipationDistance, 0.1f, 0.0f, 20.0f, "%.1fm");
						ImGui::DragFloat(" 溜め時間", &ct.chargeTime, 0.05f, 0.0f, 3.0f, "%.2f秒");
						ImGui::DragFloat(" 突進時間", &ct.rushTime, 0.05f, 0.0f, 3.0f, "%.2f秒");
						ImGui::DragFloat(" 突進速度倍率", &ct.rushSpeedMultiplier, 0.5f, 0.0f, 50.0f, "x%.1f");
						ImGui::DragFloat(" 突進ホーミング強度", &ct.rushHomingStrength, 0.1f, 0.0f, 10.0f, "%.1f");
						if (ImGui::IsItemHovered()) ImGui::SetTooltip("大きいほどプレイヤーを追尾する。1.5前後で読み避け可能");
						ImGui::DragFloat(" クールダウン", &ct.cooldownTime, 0.05f, 0.0f, 3.0f, "%.2f秒");
						ImGui::TreePop();
					}
				}

				ImGui::Separator();

				// 攻撃パターン編集セクション
				if (ImGui::TreeNode("攻撃パターン設定")) {

					ImGui::Text("現在の攻撃パターン:");

					// 現在の攻撃パターン一覧表示と削除
					for (size_t i = 0; i < data.attackPatterns.size(); ++i) {
						ImGui::PushID(static_cast<int>(i));

						ImGui::BulletText("%s", AttackPatternToString(data.attackPatterns[i]));
						ImGui::SameLine();

						if (ImGui::SmallButton("削除")) {
							data.attackPatterns.erase(data.attackPatterns.begin() + i);
							--i; // インデックス調整
							ImGui::PopID();
							continue;
						}

						// 順序変更ボタン
						if (i > 0) {
							ImGui::SameLine();
							if (ImGui::SmallButton("↑")) {
								std::swap(data.attackPatterns[i], data.attackPatterns[i - 1]);
							}
						}
						if (i < data.attackPatterns.size() - 1) {
							ImGui::SameLine();
							if (ImGui::SmallButton("↓")) {
								std::swap(data.attackPatterns[i], data.attackPatterns[i + 1]);
							}
						}

						ImGui::PopID();
					}

					if (data.attackPatterns.empty()) {
						ImGui::TextColored(ImVec4(1, 0, 0, 1), "警告: 攻撃パターンが設定されていません！");
					}

					ImGui::Separator();

					// 新しい攻撃パターンを追加
					static int selectedPatternIndex = 0;
					ImGui::Combo("追加する攻撃", &selectedPatternIndex, availablePatternNames, patternCount);

					ImGui::SameLine();
					if (ImGui::Button("追加")) {
						AttackPatternType newPattern = availablePatterns[selectedPatternIndex];

						// 重複チェック
						bool alreadyExists = false;
						for (const auto& pattern : data.attackPatterns) {
							if (pattern == newPattern) {
								alreadyExists = true;
								break;
							}
						}

						if (!alreadyExists) {
							data.attackPatterns.push_back(newPattern);
							Logger(("[BattleEnemyManager] 攻撃パターン追加: " + data.enemyId + " -> " + AttackPatternToString(newPattern) + "\n").c_str());
						} else {
							Logger((std::string("[BattleEnemyManager] 警告: ") + AttackPatternToString(newPattern) + " は既に存在します\n").c_str());
						}
					}

					ImGui::SameLine();
					if (ImGui::Button("全てクリア")) {
						data.attackPatterns.clear();
						Logger(("[BattleEnemyManager] 攻撃パターンをクリア: " + data.enemyId + "\n").c_str());
					}

					ImGui::Separator();

					// プリセットボタン
					ImGui::Text("クイック設定:");

					if (ImGui::Button("基本型 (rush)")) {
						data.attackPatterns = { AttackPatternType::Rush };
					}
					ImGui::SameLine();
					if (ImGui::Button("アグレッシブ (rush, chargeRush, combo)")) {
						data.attackPatterns = { AttackPatternType::Rush, AttackPatternType::ChargeRush, AttackPatternType::Combo };
					}

					if (ImGui::Button("トリッキー (spin)")) {
						data.attackPatterns = { AttackPatternType::Spin };
					}
					ImGui::SameLine();
					if (ImGui::Button("全種類")) {
						data.attackPatterns = { AttackPatternType::Rush, AttackPatternType::Jump, AttackPatternType::Spin,
							AttackPatternType::ChargeRush, AttackPatternType::Combo };
					}

					ImGui::TreePop();
				}

				ImGui::Separator();

				// 現在の設定を生成中の敵に適用
				if (ImGui::Button("この設定を生成中の同種敵に適用")) {
					int appliedCount = 0;
					for (auto& enemy : manager.battleEnemies_) {
						if (enemy && enemy->GetEnemyData().enemyId == data.enemyId) {
							// 生成中の敵のデータを丸ごと差し替える。
							// 以前はフィールドを1つずつ写していたため、BattleEnemyData に
							// 項目を足すたびに写し忘れて「エディタで変えても効かない」が起きていた。
							// 実行時のHPは BaseEnemy 側が持つので、丸ごとコピーしても現在HPは巻き戻らない。
							enemy->GetEnemyData() = data;
							appliedCount++;
						}
					}
					Logger(("[BattleEnemyManager] " + std::to_string(appliedCount) + "体の敵に設定を適用しました\n").c_str());
				}

				ImGui::TreePop();
			}
		}

		ImGui::Separator();

		// データ保存ボタン
		if (ImGui::Button("全ての変更をJSONに保存")) {
			if (manager.SaveEnemyData(manager.enemyDataFilePath_)) {
				Logger("[BattleEnemyManager] 敵ベースデータをJSONファイルに保存しました。\n");
			} else {
				ThrowError("[BattleEnemyManager] 敵ベースデータの保存に失敗しました。\n");
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("JSONから再読み込み")) {
			manager.LoadEnemyData(manager.enemyDataFilePath_);
		}

		ImGui::TreePop();
	}

	ImGui::Separator();

	// アクティブな敵の編集
	if (ImGui::TreeNode("アクティブな敵")) {
		for (size_t i = 0; i < manager.battleEnemies_.size(); ++i) {
			auto& enemy = manager.battleEnemies_[i];
			if (enemy) {
				std::string label = "敵 " + std::to_string(i) + " (" + enemy->GetEnemyData().enemyId + ")";
				if (ImGui::TreeNode(label.c_str())) {
					// 基本情報
					ImGui::Text("HP: %d / %d", enemy->GetCurrentHP(), enemy->GetMaxHP());
					ImGui::Text("生存: %s", enemy->IsAlive() ? "はい" : "いいえ");

					Vector3 pos = enemy->GetTranslate();
					ImGui::Text("位置: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);

					// 個体データ（テスト用に編集可能）
					BattleEnemyData& enemyData = enemy->GetEnemyData();
					ImGui::Text("敵ID: %s", enemyData.enemyId.c_str());
					ImGui::Text("モデル: %s", enemyData.modelPath.c_str());
					ImGui::Text("攻撃力: %d (Base:%d)", enemyData.attack, enemyData.attack);
					ImGui::Text("防御力: %d (Base:%d)", enemyData.defense, enemyData.defense);

					// 個体のパラメータ調整（リアルタイムテスト用）
					ImGui::DragFloat("移動速度 (Current)", &enemyData.moveSpeed, 0.1f, 0.0f, 20.0f);
					ImGui::DragFloat("攻撃状態に入る距離 (Current)", &enemyData.attackStateRange, 0.1f, 0.0f, 100.0f);
					ImGui::DragFloat("追跡状態に入る距離 (Current)", &enemyData.approachStateRange, 0.1f, 0.0f, 100.0f);

					// 間合い取り（この個体にだけ即座に効く。数値の当たりを付けるのに使う）
					if (ImGui::CollapsingHeader("間合い取り (この個体のみ・即反映)")) {
						DrawSpacingEditor(enemyData.spacing, "inst");
					}

					// 攻撃パターン表示
					ImGui::Text("攻撃パターン:");
					for (const auto& pattern : enemyData.attackPatterns) {
						ImGui::BulletText("%s", AttackPatternToString(pattern));
					}

					// 敵操作ボタン
					if (ImGui::Button("ダメージ(25)")) {
						enemy->TakeDamage(25);
					}
					ImGui::SameLine();
					if (ImGui::Button("回復(30)")) {
						enemy->Heal(30);
					}
					ImGui::SameLine();
					if (ImGui::Button("即死")) {
						enemy->TakeDamage(enemy->GetCurrentHP());
					}

					ImGui::TreePop();
				}
			}
		}
		ImGui::TreePop();
	}

	// フォーメーション
	if (ImGui::TreeNode("フォーメーション")) {
		for (const auto& pair : manager.formationMap_) {
			const auto& formation = pair.second;
			if (ImGui::TreeNode(formation.formationName.c_str())) {
				ImGui::Text("説明: %s", formation.description.c_str());
				ImGui::Text("位置数: %zu", formation.positions.size());
				for (size_t i = 0; i < formation.positions.size(); ++i) {
					const auto& pos = formation.positions[i];
					ImGui::Text("  %zu: (%.1f, %.1f, %.1f)", i, pos.x, pos.y, pos.z);
				}
				if (ImGui::Button("フォーメーション設定")) {
					manager.SetFormation(formation.formationName);
				}
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
}

#endif
