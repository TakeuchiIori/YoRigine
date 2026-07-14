#include "GpuEmitManagerEditorUI.h"
#ifdef USE_IMGUI

#include "GpuEmitManager.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <Loaders/Texture/TextureManager.h>
#include <ModelManager.h>
#include <Debugger/Logger.h>
#include <IconsFontAwesome5.h>
#include "imgui.h"

namespace YoRigine {

	void GpuEmitManagerEditorUI::DrawImGui(GpuEmitManager& manager)
	{
#ifdef USE_IMGUI
		// メニューバー

		if (ImGui::CollapsingHeader("ファイル操作（上級者向け・全グループ一括）", ImGuiTreeNodeFlags_None))
		{
			ImGui::TextDisabled("通常はここを使わず、[グループ管理]タブでグループ単位に保存してください。");
			ImGui::TextDisabled("↓は全グループをまとめて1ファイルに読み書きする一括操作です。");
			ImGui::Spacing();
			float halfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
			if (ImGui::Button(ICON_FA_SAVE " 全グループを保存", ImVec2(halfWidth, 0))) {
				if (manager.SaveToFile(manager.saveFilePath_))
					std::cout << "保存成功: " << manager.saveFilePath_ << std::endl;
				else
					std::cout << "保存失敗: " << manager.saveFilePath_ << std::endl;
			}
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_FOLDER_OPEN " 読み込み", ImVec2(halfWidth, 0))) {
				if (manager.LoadFromFile(manager.saveFilePath_))
					std::cout << "読み込み成功: " << manager.saveFilePath_ << std::endl;
				else
					std::cout << "読み込み失敗: " << manager.saveFilePath_ << std::endl;
			}

			ImGui::Separator();

			// 2. パス入力とディレクトリのスキャンロジック
			ImGui::InputText("ファイルパス", manager.saveFilePath_, sizeof(manager.saveFilePath_));

			// 編集中のパスからディレクトリ部分を抽出 (既存ロジック)
			std::filesystem::path currentPath(manager.saveFilePath_);

			// ディレクトリが変わっていたら再スキャン
			std::string dirPath = currentPath.has_filename()
				? currentPath.parent_path().string()
				: currentPath.string();
			if (!dirPath.empty() && dirPath.back() != '/') dirPath += '/';

			if (dirPath != manager.jsonBrowser_.GetCurrentDir()) {
				manager.jsonBrowser_.Scan(dirPath);
			}

			// FileBrowser に全部任せる
			manager.jsonBrowser_.Draw("JsonList", ImVec2(0, 150));

			ImGui::Separator();

			// 全削除ボタン（目立つように配置）
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // 危険な操作なので赤くする
			if (ImGui::Button("全削除", ImVec2(ImGui::GetContentRegionAvail().x, 0)) && !manager.groups_.empty()) {
				manager.showDeleteDialog_ = true;
				manager.selectedGroupName_.clear();
			}
			ImGui::PopStyleColor();
		}
		// エミッタ形状のライン可視化トグル
		ImGui::Checkbox("エミッタ形状をライン表示（選択グループ）", &manager.showEmitterGizmos_);
		ImGui::SameLine();
		ImGui::TextDisabled("(黄=選択エミッタ / 水色=同グループの他)");

		// タブバーでセクション分け
		if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_None))
		{
			// ===== グループ管理タブ =====
			if (ImGui::BeginTabItem("グループ管理"))
			{
				DrawGroupManagementTab(manager);
				ImGui::EndTabItem();
			}

			// ===== エミッター管理タブ =====
			if (ImGui::BeginTabItem("エミッター管理"))
			{
				DrawEmitterManagementTab(manager);
				ImGui::EndTabItem();
			}

			// ===== エディタータブ =====
			if (ImGui::BeginTabItem("エディター"))
			{
				DrawEditorTab(manager);
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		// 削除確認ダイアログ
		DrawDeleteDialog(manager);

#endif // USE_IMGUI
	}

	bool GpuEmitManagerEditorUI::DrawParticleParametersEditor([[maybe_unused]] GpuEmitManager& manager, GpuEmitManager::EmitterData* emitterData)
	{
#ifdef USE_IMGUI
		bool changed = false;

		if (ImGui::CollapsingHeader("パーティクルパラメータ設定", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));

			// ===== 描画メッシュ形状（1粒子の見た目）=====
			{
				static const char* meshShapeNames[] = {
					"板ポリ(Plane)", "立方体(Box)", "リング(Ring)", "円柱(Cylinder)",
					"球(Sphere)", "円錐(Cone)", "扇形(Fan)"
				};
				bool meshDirty = false;

				int shapeIdx = static_cast<int>(emitterData->particleMeshShape);
				if (ImGui::Combo("粒子メッシュ形状", &shapeIdx, meshShapeNames, IM_ARRAYSIZE(meshShapeNames))) {
					emitterData->particleMeshShape = static_cast<ParticleMeshShape>(shapeIdx);
					meshDirty = true;
				}

				// 形状ごとの生成パラメータ（実サイズは粒子scaleで拡縮）
				auto& mp = emitterData->particleMeshParams;
				int divideI = static_cast<int>(mp.divide);
				int subdivI = static_cast<int>(mp.subdivisions);
				switch (emitterData->particleMeshShape) {
				case ParticleMeshShape::Plane:
					meshDirty |= ImGui::DragFloat("幅",   &mp.width,  0.01f, 0.01f, 100.0f, "%.3f");
					meshDirty |= ImGui::DragFloat("高さ", &mp.height, 0.01f, 0.01f, 100.0f, "%.3f");
					break;
				case ParticleMeshShape::Box:
					meshDirty |= ImGui::DragFloat("幅",     &mp.width,  0.01f, 0.01f, 100.0f, "%.3f");
					meshDirty |= ImGui::DragFloat("高さ",   &mp.height, 0.01f, 0.01f, 100.0f, "%.3f");
					meshDirty |= ImGui::DragFloat("奥行き", &mp.depth,  0.01f, 0.01f, 100.0f, "%.3f");
					break;
				case ParticleMeshShape::Ring:
					meshDirty |= ImGui::DragFloat("外周半径", &mp.outerRadius, 0.01f, 0.01f, 100.0f, "%.3f");
					meshDirty |= ImGui::DragFloat("内周半径", &mp.innerRadius, 0.01f, 0.0f,  100.0f, "%.3f");
					if (ImGui::DragInt("分割数", &divideI, 1, 3, 128)) { mp.divide = static_cast<uint32_t>(divideI); meshDirty = true; }
					break;
				case ParticleMeshShape::Cylinder:
					meshDirty |= ImGui::DragFloat("外周半径", &mp.outerRadius, 0.01f, 0.01f, 100.0f, "%.3f");
					meshDirty |= ImGui::DragFloat("内周半径", &mp.innerRadius, 0.01f, 0.0f,  100.0f, "%.3f");
					meshDirty |= ImGui::DragFloat("高さ",     &mp.height,      0.01f, 0.01f, 100.0f, "%.3f");
					if (ImGui::DragInt("分割数", &divideI, 1, 3, 128)) { mp.divide = static_cast<uint32_t>(divideI); meshDirty = true; }
					break;
				case ParticleMeshShape::Sphere:
					meshDirty |= ImGui::DragFloat("半径", &mp.radius, 0.01f, 0.01f, 100.0f, "%.3f");
					if (ImGui::DragInt("細分化レベル", &subdivI, 1, 0, 5)) { mp.subdivisions = static_cast<uint32_t>(subdivI); meshDirty = true; }
					break;
				case ParticleMeshShape::Cone:
					meshDirty |= ImGui::DragFloat("半径", &mp.radius, 0.01f, 0.01f, 100.0f, "%.3f");
					meshDirty |= ImGui::DragFloat("高さ", &mp.height, 0.01f, 0.01f, 100.0f, "%.3f");
					if (ImGui::DragInt("分割数", &divideI, 1, 3, 128)) { mp.divide = static_cast<uint32_t>(divideI); meshDirty = true; }
					break;
				case ParticleMeshShape::Fan:
					meshDirty |= ImGui::DragFloat("半径",     &mp.radius,      0.01f, 0.01f, 100.0f, "%.3f");
					meshDirty |= ImGui::DragFloat("扇の角度", &mp.angleDegree, 1.0f,  1.0f,  360.0f, "%.1f°");
					if (ImGui::DragInt("分割数", &divideI, 1, 3, 128)) { mp.divide = static_cast<uint32_t>(divideI); meshDirty = true; }
					break;
				}

				// 形状 or パラメータが変わったらメッシュを再生成
				if (meshDirty && emitterData->emitter) {
					emitterData->emitter->SetParticleMesh(emitterData->particleMeshShape, emitterData->particleMeshParams);
					changed = true;
				}

				if (emitterData->particleMeshShape != ParticleMeshShape::Plane) {
					ImGui::TextDisabled("立体メッシュは「ビルボード」をOFFにすると向きが分かりやすくなります");
				}
				ImGui::Spacing();
			}

			// ===== ビルボード設定 =====
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.5f, 0.7f, 0.8f));
			if (ImGui::CollapsingHeader("ビルボード", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PopStyleColor();
				ImGui::Indent(16.0f);
				changed |= ImGui::Checkbox("ビルボードを有効", &emitterData->particleParams.isBillboard);
				ImGui::TextDisabled("パーティクルが常にカメラの方向を向きます");
				ImGui::Unindent(16.0f);
				ImGui::Spacing();
			} else
			{
				ImGui::PopStyleColor();
			}

			// ===== 生存時間 =====
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.7f, 0.3f, 0.3f, 0.8f));
			if (ImGui::CollapsingHeader("生存時間", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PopStyleColor();
				ImGui::Indent(16.0f);

				changed |= ImGui::DragFloat("基本時間 (秒)", &emitterData->particleParams.lifeTime,
					0.1f, 0.1f, 30.0f, "%.2f 秒");

				changed |= ImGui::DragFloat("ランダム生存幅 (±)", &emitterData->particleParams.lifeTimeVariance,
					0.01f, 0.0f, 10.0f, "± %.2f 秒");

				float minLife = emitterData->particleParams.lifeTime - emitterData->particleParams.lifeTimeVariance;
				float maxLife = emitterData->particleParams.lifeTime + emitterData->particleParams.lifeTimeVariance;

				ImGui::BeginDisabled();
				ImGui::Text("範囲: %.2f ~ %.2f 秒", minLife, maxLife);
				ImGui::EndDisabled();

				ImGui::Unindent(16.0f);
				ImGui::Spacing();
			} else
			{
				ImGui::PopStyleColor();
			}

			// ===== スケール =====
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.7f, 0.3f, 0.8f));
			if (ImGui::CollapsingHeader("スケール", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PopStyleColor();
				ImGui::Indent(16.0f);

				// 基本スケール
				changed |= ImGui::DragFloat3("開始スケール", &emitterData->particleParams.startScale.x,
					0.01f, 0.01f, 100.0f, "%.2f");

				// ランダム幅
				changed |= ImGui::DragFloat3("開始ランダムスケール幅", &emitterData->particleParams.startScaleVariance.x,
					0.01f, 0.0f, 50.0f, "± %.2f");
				ImGui::Spacing();

				// 終了スケール
				changed |= ImGui::DragFloat3("終了スケール", &emitterData->particleParams.endScale.x,
					0.01f, 0.0f, 100.0f, "%.2f");
				// ランダム幅
				changed |= ImGui::DragFloat3("終了ランダムスケール幅", &emitterData->particleParams.endScaleVariance.x,
					0.01f, 0.0f, 50.0f, "± %.2f");

				ImGui::Unindent(16.0f);
				ImGui::Spacing();
			} else
			{
				ImGui::PopStyleColor();
			}

			// ===== 回転 =====
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.7f, 0.5f, 0.2f, 0.8f));
			if (ImGui::CollapsingHeader("回転", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PopStyleColor();
				ImGui::Indent(16.0f);

				// 初期回転角度（ラジアン）
				float rotationDeg = emitterData->particleParams.rotation * (180.0f / 3.14159265f);
				if (ImGui::DragFloat("初期回転角度", &rotationDeg, 1.0f, -360.0f, 360.0f, "%.1f°"))
				{
					emitterData->particleParams.rotation = rotationDeg * (3.14159265f / 180.0f);
					changed = true;
				}

				// 初期回転のランダム幅（ラジアン）
				float rotationVarianceDeg = emitterData->particleParams.rotationVariance * (180.0f / 3.14159265f);
				if (ImGui::DragFloat("ランダム回転幅", &rotationVarianceDeg, 1.0f, 0.0f, 180.0f, "± %.1f°"))
				{
					emitterData->particleParams.rotationVariance = rotationVarianceDeg * (3.14159265f / 180.0f);
					changed = true;
				}

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				// 回転速度（ラジアン/秒）
				float rotationSpeedDeg = emitterData->particleParams.rotationSpeed * (180.0f / 3.14159265f);
				if (ImGui::DragFloat("回転速度", &rotationSpeedDeg, 1.0f, -360.0f, 360.0f, "%.1f°/s"))
				{
					emitterData->particleParams.rotationSpeed = rotationSpeedDeg * (3.14159265f / 180.0f);
					changed = true;
				}

				// 回転速度のランダム幅（ラジアン/秒）
				float rotationSpeedVarianceDeg = emitterData->particleParams.rotationSpeedVariance * (180.0f / 3.14159265f);
				if (ImGui::DragFloat("ランダム回転速度幅", &rotationSpeedVarianceDeg, 1.0f, 0.0f, 180.0f, "± %.1f°/s"))
				{
					emitterData->particleParams.rotationSpeedVariance = rotationSpeedVarianceDeg * (3.14159265f / 180.0f);
					changed = true;
				}

				// プリセットボタン
				ImGui::Spacing();
				ImGui::TextDisabled("回転プリセット:");
				if (ImGui::Button("回転しない")) {
					emitterData->particleParams.rotationSpeed = 0.0f;
					emitterData->particleParams.rotationSpeedVariance = 0.0f;
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("ゆっくり右回転")) {
					emitterData->particleParams.rotationSpeed = 0.5f;
					emitterData->particleParams.rotationSpeedVariance = 0.1f;
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("速く右回転")) {
					emitterData->particleParams.rotationSpeed = 2.0f;
					emitterData->particleParams.rotationSpeedVariance = 0.5f;
					changed = true;
				}
				if (ImGui::Button("ゆっくり左回転")) {
					emitterData->particleParams.rotationSpeed = -0.5f;
					emitterData->particleParams.rotationSpeedVariance = 0.1f;
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("速く左回転")) {
					emitterData->particleParams.rotationSpeed = -2.0f;
					emitterData->particleParams.rotationSpeedVariance = 0.5f;
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("ランダム回転")) {
					emitterData->particleParams.rotationSpeed = 0.0f;
					emitterData->particleParams.rotationSpeedVariance = 2.0f;
					changed = true;
				}

				// プレビュー表示
				ImGui::Spacing();
				ImGui::BeginDisabled();
				float minRotSpeed = (emitterData->particleParams.rotationSpeed - emitterData->particleParams.rotationSpeedVariance) * (180.0f / 3.14159265f);
				float maxRotSpeed = (emitterData->particleParams.rotationSpeed + emitterData->particleParams.rotationSpeedVariance) * (180.0f / 3.14159265f);
				ImGui::Text("回転速度の範囲: %.1f° ~ %.1f° per second", minRotSpeed, maxRotSpeed);
				ImGui::EndDisabled();

				ImGui::Unindent(16.0f);
				ImGui::Spacing();
			} else
			{
				ImGui::PopStyleColor();
			}

			// ===== 速度 =====
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.5f, 0.3f, 0.7f, 0.8f));
			if (ImGui::CollapsingHeader("速度", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PopStyleColor();
				ImGui::Indent(16.0f);

				// 基本速度
				changed |= ImGui::DragFloat3("基本速度", &emitterData->particleParams.velocity.x,
					0.01f, -10.0f, 10.0f, "%.2f");

				// ランダム幅
				changed |= ImGui::DragFloat3("ランダム速度幅", &emitterData->particleParams.velocityVariance.x,
					0.01f, 0.0f, 5.0f, "± %.2f");

				// 方向プリセット
				ImGui::Spacing();
				ImGui::TextDisabled("方向プリセット:");
				if (ImGui::Button("上")) {
					emitterData->particleParams.velocity = Vector3(0.0f, 1.0f, 0.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("下")) {
					emitterData->particleParams.velocity = Vector3(0.0f, -1.0f, 0.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("前")) {
					emitterData->particleParams.velocity = Vector3(0.0f, 0.0f, 1.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("後ろ")) {
					emitterData->particleParams.velocity = Vector3(0.0f, 0.0f, -1.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("右")) {
					emitterData->particleParams.velocity = Vector3(1.0f, 0.0f, 0.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("左")) {
					emitterData->particleParams.velocity = Vector3(-1.0f, 0.0f, 0.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("停止")) {
					emitterData->particleParams.velocity = Vector3(0.0f, 0.0f, 0.0f);
					changed = true;
				}

				// 速度の大きさを表示
				ImGui::Spacing();
				ImGui::BeginDisabled();
				float speed = std::sqrt(
					emitterData->particleParams.velocity.x * emitterData->particleParams.velocity.x +
					emitterData->particleParams.velocity.y * emitterData->particleParams.velocity.y +
					emitterData->particleParams.velocity.z * emitterData->particleParams.velocity.z
				);
				ImGui::Text("速度の大きさ: %.2f units/sec", speed);
				ImGui::EndDisabled();

				ImGui::Unindent(16.0f);
				ImGui::Spacing();
			} else
			{
				ImGui::PopStyleColor();
			}

			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(1.0f, 0.2f, 0.0f, 1.0f));
			if (ImGui::CollapsingHeader("物理設定", ImGuiTreeNodeFlags_None))
			{
				ImGui::PopStyleColor();
				ImGui::Indent(16.0f);
				ImGui::DragFloat("重力影響度", &emitterData->particleParams.gravity,0.01f, -10.0f, 10.0f, "%.2f");
				ImGui::Unindent(16.0f);
			}
			else {
				ImGui::PopStyleColor();
			}

			// ===== 色 =====
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.7f, 0.7f, 0.2f, 0.8f));
			if (ImGui::CollapsingHeader("色", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PopStyleColor();
				ImGui::Indent(16.0f);

				// 基本色（カラーピッカー）
				changed |= ImGui::ColorEdit4("開始色", &emitterData->particleParams.startColor.x,
					ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB);

				// ランダム幅（RGB）
				changed |= ImGui::DragFloat3("開始色 RGB ランダム幅(±)", &emitterData->particleParams.startColorVariance.x,
					0.01f, 0.0f, 1.0f, "± %.2f");

				// アルファのランダム幅
				changed |= ImGui::DragFloat("開始色 Alpha ランダム幅 (±)", &emitterData->particleParams.startColorVariance.w,
					0.01f, 0.0f, 1.0f, "± %.2f");
				ImGui::Spacing();

				// 終了色（カラーピッカー）
				changed |= ImGui::ColorEdit4("終了色", &emitterData->particleParams.endColor.x,
					ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB);
				// ランダム幅（RGB）
				changed |= ImGui::DragFloat3("終了色 RGB ランダム幅(±)", &emitterData->particleParams.endColorVariance.x,
					0.01f, 0.0f, 1.0f, "± %.2f");
				// アルファのランダム幅
				changed |= ImGui::DragFloat("終了色 Alpha ランダム幅 (±)", &emitterData->particleParams.endColorVariance.w,
					0.01f, 0.0f, 1.0f, "± %.2f");

				ImGui::Unindent(16.0f);
				ImGui::Spacing();
			} else
			{
				ImGui::PopStyleColor();
			}
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
			if (ImGui::CollapsingHeader("トレイル（粒子が動いた軌跡に子パーティクルを生成）", ImGuiTreeNodeFlags_None))
			{
				ImGui::PopStyleColor();
				ImGui::PushID("ChildTrail");
				changed |= ImGui::Checkbox("有効化", &emitterData->particleParams.child.isTrail);
				changed |= ImGui::Checkbox("親のスケールを継承", &emitterData->particleParams.child.isInheritScale);
				changed |= ImGui::DragFloat("寿命", &emitterData->particleParams.child.lifeTime, 0.01f, 0.01f, 30.0f);
				changed |= ImGui::DragFloat("生成距離", &emitterData->particleParams.child.minDistance, 0.01f, 0.01f, 1000.0f);
				changed |= ImGui::DragFloat("開始スケール", &emitterData->particleParams.child.startScale, 0.01f, 0.01f, 100.0f);
				changed |= ImGui::DragFloat("終了スケール", &emitterData->particleParams.child.endScale, 0.01f, 0.0f, 100.0f);
				changed |= ImGui::DragInt("1回の生成数", &emitterData->particleParams.child.emissionCount, 1.0f, 1, 100);
				ImGui::PopID();
			} else {
				ImGui::PopStyleColor();
			}

			// ===== プリセット全体 =====
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 0.3f, 0.8f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.7f, 0.4f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));

			ImGui::Text("パーティクルプリセットs:");
			// 未実装
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar(2);
		}

		// ===== フォースフィールド =====
		ImGui::Spacing();
		ImGui::Separator();
		if (ImGui::CollapsingHeader("フォースフィールド")) {
			const char* shapeNames[] = { "Sphere", "AABB" };
			const char* modeNames[]  = { "DirectionalAccel", "ConvergeToCenter", "RadialRepel" };

			auto& ffs = emitterData->forceFields;
			for (int i = 0; i < static_cast<int>(ffs.size()); i++) {
				auto& ff = ffs[i];
				ImGui::PushID(i);
				char label[64];
				snprintf(label, sizeof(label), "Field %d###FF%d", i, i);
				if (ImGui::TreeNode(label)) {
					changed |= ImGui::Checkbox("有効", &ff.isEnable);
					int shapeIdx = static_cast<int>(ff.shape);
					if (ImGui::Combo("形状", &shapeIdx, shapeNames, 2)) { ff.shape = static_cast<GpuFieldShape>(shapeIdx); changed = true; }
					changed |= ImGui::DragFloat3("中心", &ff.center.x, 0.1f);
					if (ff.shape == GpuFieldShape::Sphere)
						changed |= ImGui::DragFloat("半径", &ff.radius, 0.1f, 0.1f, 100.f);
					else
						changed |= ImGui::DragFloat3("半サイズ(AABB)", &ff.halfExtents.x, 0.1f, 0.1f, 100.f);
					int modeIdx = static_cast<int>(ff.mode);
					if (ImGui::Combo("モード", &modeIdx, modeNames, 3)) { ff.mode = static_cast<GpuFieldMode>(modeIdx); changed = true; }
					if (ff.mode == GpuFieldMode::DirectionalAccel)
						changed |= ImGui::DragFloat3("方向", &ff.direction.x, 0.01f, -1.f, 1.f);
					changed |= ImGui::DragFloat("強度", &ff.strength, 0.1f, 0.0f, 500.f);
					changed |= ImGui::DragFloat("距離減衰(falloff)", &ff.falloff, 0.01f, 0.0f, 1.f);
					if (ff.mode == GpuFieldMode::ConvergeToCenter) {
						changed |= ImGui::DragFloat("螺旋Min", &ff.spiralStrengthMin, 0.01f, 0.0f, 10.f);
						changed |= ImGui::DragFloat("螺旋Max", &ff.spiralStrengthMax, 0.01f, 0.0f, 10.f);
						changed |= ImGui::DragFloat("ランダム軸ブレンド", &ff.randomAxisBlend, 0.01f, 0.0f, 1.f);
						changed |= ImGui::DragFloat("軌道保持率", &ff.orbitHoldRatio, 0.01f, 0.0f, 1.f);
						changed |= ImGui::DragFloat("収束遅延ばらつき", &ff.approachVariance, 0.01f, 0.0f, 1.f);
						changed |= ImGui::DragFloat("killRadius", &ff.killRadius, 0.01f, 0.0f, 20.f);
					}
					changed |= ImGui::DragFloat("最大速度(0=無制限)", &ff.maxSpeed, 0.1f, 0.0f, 200.f);
					if (ImGui::Button("削除")) { ffs.erase(ffs.begin() + i); changed = true; ImGui::TreePop(); ImGui::PopID(); break; }
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			if (static_cast<int>(ffs.size()) < static_cast<int>(GPUEmitter::kMaxForceFields)) {
				if (ImGui::Button("+ フィールド追加")) { ffs.emplace_back(); changed = true; }
			} else {
				ImGui::TextDisabled("最大 %u 個まで", GPUEmitter::kMaxForceFields);
			}
		}

		return changed;
#else
		(void)emitterData;
		return false;
#endif
	}

	/// <summary>
	/// 現在の形状に応じたパラメータ編集を表示
	/// </summary>
	bool GpuEmitManagerEditorUI::DrawShapeEditor(GpuEmitManager& manager, GpuEmitManager::EmitterData* emitterData)
	{
		switch (emitterData->shape)
		{
		case EmitterShape::Sphere:   return DrawSphereEditor(manager, emitterData);
		case EmitterShape::Box:      return DrawBoxEditor(manager, emitterData);
		case EmitterShape::Triangle: return DrawTriangleEditor(manager, emitterData);
		case EmitterShape::Cone:     return DrawConeEditor(manager, emitterData);
		case EmitterShape::Mesh:     return DrawMeshEditor(manager, emitterData);
		}
		return false;
	}

	/// <summary>
	/// Sphere パラメータの ImGui 編集
	/// </summary>
	bool GpuEmitManagerEditorUI::DrawSphereEditor([[maybe_unused]] GpuEmitManager& manager, GpuEmitManager::EmitterData* emitterData)
	{
		(void)emitterData;
		bool changed = false;
#ifdef USE_IMGUI

		auto& p = emitterData->sphereParams;

		changed |= ImGui::DragFloat3("位置", &p.translate.x, 0.1f);
		changed |= ImGui::DragFloat("半径", &p.radius, 0.1f, 0.1f, 10000.0f);
		changed |= ImGui::DragFloat("射出パーティクル数", &p.count, 1.0f, 1.0f, GPUParticle::kMaxParticles);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f, 0.01f, 10.0f);


#endif
		return changed;
	}

	/// <summary>
	/// Box パラメータの ImGui 編集
	/// </summary>
	bool GpuEmitManagerEditorUI::DrawBoxEditor([[maybe_unused]] GpuEmitManager& manager, GpuEmitManager::EmitterData* emitterData)
	{
		(void)emitterData;
		bool changed = false;
#ifdef USE_IMGUI

		auto& p = emitterData->boxParams;

		changed |= ImGui::DragFloat3("位置", &p.translate.x, 0.1f);
		changed |= ImGui::DragFloat3("サイズ", &p.size.x, 0.1f, 0.1f, 10000.0f);
		changed |= ImGui::DragFloat("射出パーティクル数", &p.count, 1.0f, 1.0f, GPUParticle::kMaxParticles);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f, 0.01f, 10.0f);


#endif
		return changed;
	}

	/// <summary>
	/// Triangle パラメータの ImGui 編集
	/// </summary>
	bool GpuEmitManagerEditorUI::DrawTriangleEditor([[maybe_unused]] GpuEmitManager& manager, GpuEmitManager::EmitterData* emitterData)
	{
		(void)emitterData;
		bool changed = false;
#ifdef USE_IMGUI

		auto& p = emitterData->triangleParams;

		changed |= ImGui::DragFloat3("頂点 1", &p.v1.x, 0.1f);
		changed |= ImGui::DragFloat3("頂点 2", &p.v2.x, 0.1f);
		changed |= ImGui::DragFloat3("頂点 3", &p.v3.x, 0.1f);
		changed |= ImGui::DragFloat("射出パーティクル数", &p.count, 1.0f, 1.0f, GPUParticle::kMaxParticles);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f, 0.01f, 10.0f);
#endif
		return changed;
	}

	/// <summary>
	/// Cone パラメータの ImGui 編集
	/// </summary>
	bool GpuEmitManagerEditorUI::DrawConeEditor([[maybe_unused]] GpuEmitManager& manager, GpuEmitManager::EmitterData* emitterData)
	{
		(void)emitterData;
		bool changed = false;
#ifdef USE_IMGUI

		auto& p = emitterData->coneParams;

		changed |= ImGui::DragFloat3("位置", &p.translate.x, 0.1f);
		changed |= ImGui::DragFloat3("とんがる方向", &p.direction.x, 0.01f, -1.0f, 1.0f);
		changed |= ImGui::DragFloat("半径", &p.radius, 0.1f, 0.1f, 10000.0f);
		changed |= ImGui::DragFloat("高さ", &p.height, 0.1f, 0.1f, 10000.0f);
		changed |= ImGui::DragFloat("射出パーティクル数", &p.count, 1.0f, 1.0f, GPUParticle::kMaxParticles);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f, 0.01f, 10.0f);

#endif
		return changed;
	}

	bool GpuEmitManagerEditorUI::DrawMeshEditor(GpuEmitManager& manager, GpuEmitManager::EmitterData* emitterData)
	{
		(void)emitterData;
#ifdef USE_IMGUI
		bool changed = false;
		auto& p = emitterData->meshParams;

		// -------------------------
		// モデル選択コンボボックス
		// -------------------------
		assert(manager.modelManager_ && "GpuEmitManager : SetModelManager() を先に呼ぶこと");
		auto modelKeys = manager.modelManager_->GetModelKeys();
		static int selected = -1;

		// 現在の選択を反映
		if (p.model != nullptr) {
			std::string currentKey = p.model->GetName();
			for (int i = 0; i < modelKeys.size(); i++) {
				if (modelKeys[i] == currentKey) {
					selected = i;
					break;
				}
			}
		}

		if (ImGui::BeginCombo("使用モデル", selected >= 0 ? modelKeys[selected].c_str() : "未選択"))
		{
			for (int i = 0; i < modelKeys.size(); i++)
			{
				bool isSelected = (selected == i);
				if (ImGui::Selectable(modelKeys[i].c_str(), isSelected)) {
					selected = i;
					p.model = manager.modelManager_->FindModel(modelKeys[i]);
					changed = true;
				}
			}
			ImGui::EndCombo();
		}

		// -------------------------
		// 通常パラメータ
		// -------------------------

		changed |= ImGui::DragFloat3("位置", &p.translate.x, 0.1f);
		changed |= ImGui::DragFloat3("スケール", &p.scale.x, 0.1f);

		float r[4] = { p.rotation.x, p.rotation.y, p.rotation.z, p.rotation.w };
		if (ImGui::DragFloat4("回転(Quat)", r, 0.01f)) {
			p.rotation = Quaternion(r[0], r[1], r[2], r[3]);
			changed = true;
		}

		changed |= ImGui::DragFloat("射出数", &p.count, 1.0f);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f);

		const char* modeList[] = { "Surface", "Volume", "Edge" };
		int modeIndex = static_cast<int>(p.emitMode);
		if (ImGui::Combo("Emit Mode", &modeIndex, modeList, 3)) {
			p.emitMode = static_cast<MeshEmitMode>(modeIndex);
			changed = true;
		}

		return changed;
#else
		return false;
#endif
	}

	/// <summary>
	/// グループ管理タブ
	/// </summary>
	void GpuEmitManagerEditorUI::DrawGroupManagementTab(GpuEmitManager& manager)
	{
#ifdef USE_IMGUI
		ImGui::BeginChild("GroupManagement", ImVec2(0, 0), false);

		// ===== 新規グループ作成 =====
		ImGui::SeparatorText("新規グループ作成");
		ImGui::TextDisabled("名前を入れて「＋作成」だけ。保存先は名前から自動決定（JSONの事前選択は不要）。");

		ImGui::PushItemWidth(-150);
		ImGui::InputTextWithHint("##NewGroupName", "グループ名を入力...", manager.newGroupName_, sizeof(manager.newGroupName_));
		ImGui::PopItemWidth();

		ImGui::SameLine();
		ImGui::BeginDisabled(strlen(manager.newGroupName_) == 0);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.6f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.35f, 1.0f));
		if (ImGui::Button(ICON_FA_PLUS " 作成", ImVec2(140, 0))) {
			if (manager.CreateEmitterGroup(manager.newGroupName_)) {
				manager.selectedGroupName_ = manager.newGroupName_;
			}
			manager.newGroupName_[0] = '\0';
		}
		ImGui::PopStyleColor(2);
		ImGui::EndDisabled();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// ===== グループリスト =====
		ImGui::SeparatorText("グループリスト");

		ImGui::Text("登録グループ数: %zu", manager.groups_.size());

		// フィルター検索
		static char groupFilter[256] = "";
		ImGui::PushItemWidth(-1);
		ImGui::InputTextWithHint("##GroupFilter", ICON_FA_SEARCH " 検索...", groupFilter, sizeof(groupFilter));
		ImGui::PopItemWidth();

		ImGui::Spacing();

		// グループリストテーブル
		if (ImGui::BeginTable("GroupTable", 4,
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_Resizable,
			ImVec2(0, 300)))
		{
			ImGui::TableSetupColumn("状態", ImGuiTableColumnFlags_WidthFixed, 50);
			ImGui::TableSetupColumn("グループ名", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("エミッター数", ImGuiTableColumnFlags_WidthFixed, 100);
			ImGui::TableSetupColumn("再生", ImGuiTableColumnFlags_WidthFixed, 80);
			ImGui::TableHeadersRow();

			for (auto& [name, groupData] : manager.groups_)
			{
				// フィルター適用
				if (strlen(groupFilter) > 0 && name.find(groupFilter) == std::string::npos)
					continue;

				ImGui::TableNextRow();

				// 状態列
				ImGui::TableSetColumnIndex(0);
				ImGui::PushID(name.c_str());
				ImGui::Checkbox("##Active", &groupData->isActive);
				ImGui::PopID();

				// グループ名列
				ImGui::TableSetColumnIndex(1);
				bool isSelected = (manager.selectedGroupName_ == name);

				ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns;
				if (ImGui::Selectable(name.c_str(), isSelected, flags)) {
					manager.selectedGroupName_ = name;
					manager.selectedEmitterName_.clear();
				}

				// 右クリックメニュー
				if (ImGui::BeginPopupContextItem())
				{
					if (ImGui::MenuItem("削除")) {
						manager.DeleteEmitterGroup(name);
						ImGui::EndPopup();
						break;
					}
					ImGui::EndPopup();
				}

				// エミッター数列
				ImGui::TableSetColumnIndex(2);
				ImGui::TextDisabled("%zu", groupData->emitters.size());

				// 再生状態列
				ImGui::TableSetColumnIndex(3);
				ImGui::PushID((name + "_play").c_str());
				if (groupData->isPlaying) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
					if (ImGui::SmallButton(ICON_FA_PLAY)) {
						manager.StopEmitterGroup(name);
					}
					ImGui::PopStyleColor();
				} else {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
					if (ImGui::SmallButton(ICON_FA_PLAY)) {
						manager.PlayEmitterGroup(name);
					}
					ImGui::PopStyleColor();
				}
				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		ImGui::Spacing();

		// ===== 選択グループの詳細 =====
		GpuEmitManager::EmitterGroup* currentGroup = manager.GetGroup(manager.selectedGroupName_);
		if (currentGroup)
		{
			ImGui::Separator();
			ImGui::SeparatorText(("選択中: " + currentGroup->name).c_str());

			// プロパティグリッド風のレイアウト
			if (ImGui::BeginTable("GroupProperties", 2, ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("プロパティ", ImGuiTableColumnFlags_WidthFixed, 150);
				ImGui::TableSetupColumn("値", ImGuiTableColumnFlags_WidthStretch);

				// 有効/無効
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("有効");
				ImGui::TableSetColumnIndex(1);
				ImGui::Checkbox("##GroupActive", &currentGroup->isActive);

				// 再生状態
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("再生状態");
				ImGui::TableSetColumnIndex(1);

				if (currentGroup->isPlaying) {
					ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "● 再生中");
					ImGui::SameLine();
					if (ImGui::Button(ICON_FA_STOP " 停止")) {
						manager.StopEmitterGroup(manager.selectedGroupName_);
					}
				} else {
					ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "○ 停止中");
					ImGui::SameLine();
					if (ImGui::Button(ICON_FA_PLAY " 再生")) {
						manager.PlayEmitterGroup(manager.selectedGroupName_);
					}
				}

				// 経過時間
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("経過時間");
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.2f 秒", currentGroup->currentTime);

				// システム寿命
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("システム寿命");
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);
				ImGui::DragFloat("##SystemDuration", &currentGroup->systemDuration,
					0.1f, 0.0f, 60.0f, "%.1f 秒 (0=無限)");

				// 位置
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("位置");
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);
				ImGui::DragFloat3("##GroupTranslate", &currentGroup->translate.x, 0.1f);

				ImGui::EndTable();
			}

			ImGui::Spacing();

			// 保存先ファイル（このグループ専用。1ファイル1グループが基本）
			ImGui::TextDisabled("保存先: %s",
				currentGroup->sourceFilePath.empty() ? "(未設定→保存時に自動決定)" : currentGroup->sourceFilePath.c_str());

			// グループ単位保存（主導線）
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.6f, 0.3f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.35f, 1.0f));
			if (ImGui::Button(ICON_FA_SAVE " このグループを保存", ImVec2(-1, 32))) {
				manager.SaveGroupToFile(manager.selectedGroupName_);
			}
			ImGui::PopStyleColor(2);

			ImGui::Spacing();

			// グループ操作ボタン
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 0.8f));
			if (ImGui::Button("このグループを削除", ImVec2(-1, 0))) {
				manager.showDeleteDialog_ = true;
			}
			ImGui::PopStyleColor();
		}

		ImGui::EndChild();
#endif
	}

	/// <summary>
	/// エミッター管理タブ
	/// </summary>
	void GpuEmitManagerEditorUI::DrawEmitterManagementTab(GpuEmitManager& manager)
	{
#ifdef USE_IMGUI
		ImGui::BeginChild("EmitterManagement", ImVec2(0, 0), false);

		if (manager.selectedGroupName_.empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
				"⚠ グループを選択してください");
			ImGui::Text("「グループ管理」タブでグループを作成・選択してください。");
			ImGui::EndChild();
			return;
		}

		GpuEmitManager::EmitterGroup* currentGroup = manager.GetGroup(manager.selectedGroupName_);
		if (!currentGroup) {
			ImGui::EndChild();
			return;
		}

		// ===== 新規エミッター作成 =====
		ImGui::SeparatorText("新規エミッター作成");
		ImGui::Text("作成先グループ: %s", currentGroup->name.c_str());

		ImGui::Spacing();

		// 名前入力
		ImGui::Text("名前:");
		ImGui::SameLine();
		ImGui::PushItemWidth(250);
		ImGui::InputTextWithHint("##EmitterName", "エミッター名...",
			manager.newEmitterName_, sizeof(manager.newEmitterName_));
		ImGui::PopItemWidth();

		// 形状選択
		ImGui::Text("形状:");
		ImGui::SameLine();
		ImGui::PushItemWidth(150);
		ImGui::Combo("##Shape", &manager.selectedShapeIndex_, GpuEmitManager::shapeNames_, (static_cast<int>(EmitterShape::Mesh) + 1));
		ImGui::PopItemWidth();

		// テクスチャパス
		ImGui::Text("テクスチャ:");
		ImGui::PushItemWidth(-150);
		ImGui::InputTextWithHint("##TexturePath", "テクスチャパス...",
			manager.newEmitterTexturePath_, sizeof(manager.newEmitterTexturePath_));
		ImGui::PopItemWidth();

		ImGui::SameLine();
		static bool textureBrowserOpen = false;
		if (ImGui::Button("参照...", ImVec2(140, 0))) {
			manager.ScanTextureDirectory("Resources/Textures/");
			textureBrowserOpen = !textureBrowserOpen;
		}

		// テクスチャブラウザ
		if (textureBrowserOpen)
		{
			ImGui::Spacing();
			DrawTextureBrowser(manager, textureBrowserOpen);
		}

		ImGui::Spacing();

		// 作成ボタン
		ImGui::BeginDisabled(strlen(manager.newEmitterName_) == 0);
		if (ImGui::Button("エミッター作成", ImVec2(-1, 35)))
		{
			std::string name = manager.newEmitterName_;
			std::string texPath = manager.newEmitterTexturePath_;
			EmitterShape shape = static_cast<EmitterShape>(manager.selectedShapeIndex_);

			if (manager.CreateEmitter(manager.selectedGroupName_, name, texPath, shape))
			{
				manager.selectedEmitterName_ = name;
				std::memset(manager.newEmitterName_, 0, sizeof(manager.newEmitterName_));
				std::memset(manager.newEmitterTexturePath_, 0, sizeof(manager.newEmitterTexturePath_));
			}
		}
		ImGui::EndDisabled();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// ===== エミッターリスト =====
		ImGui::SeparatorText("エミッターリスト");
		ImGui::Text("エミッター数: %zu", currentGroup->emitters.size());

		// フィルター検索
		static char emitterFilter[256] = "";
		ImGui::PushItemWidth(-1);
		ImGui::InputTextWithHint("##EmitterFilter", ICON_FA_SEARCH " 検索...",
			emitterFilter, sizeof(emitterFilter));
		ImGui::PopItemWidth();

		ImGui::Spacing();

		// エミッターリストテーブル
		if (ImGui::BeginTable("EmitterTable", 4,
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_Resizable,
			ImVec2(0, -1)))
		{
			ImGui::TableSetupColumn("有効", ImGuiTableColumnFlags_WidthFixed, 50);
			ImGui::TableSetupColumn("名前", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("形状", ImGuiTableColumnFlags_WidthFixed, 100);
			ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, 60);
			ImGui::TableHeadersRow();

			for (auto it = currentGroup->emitters.begin();
				it != currentGroup->emitters.end(); )
			{
				const std::string& name = it->first;
				auto* data = it->second.get();

				// フィルター適用
				if (strlen(emitterFilter) > 0 && name.find(emitterFilter) == std::string::npos) {
					++it;
					continue;
				}

				ImGui::TableNextRow();
				ImGui::PushID(name.c_str());

				// 有効列
				ImGui::TableSetColumnIndex(0);
				ImGui::Checkbox("##Active", &data->isActive);

				// 名前列
				ImGui::TableSetColumnIndex(1);
				bool isSelected = (manager.selectedEmitterName_ == name);

				if (ImGui::Selectable(name.c_str(), isSelected,
					ImGuiSelectableFlags_SpanAllColumns))
				{
					manager.selectedEmitterName_ = name;
				}

				// 形状列
				ImGui::TableSetColumnIndex(2);
				ImGui::TextDisabled("%s", GpuEmitManager::shapeNames_[static_cast<int>(data->shape)]);

				// 操作列
				ImGui::TableSetColumnIndex(3);
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 0.8f));
				if (ImGui::SmallButton("削除")) {
					if (manager.selectedEmitterName_ == name) {
						manager.selectedEmitterName_.clear();
					}
					it = currentGroup->emitters.erase(it);
					ImGui::PopStyleColor();
					ImGui::PopID();
					continue;
				}
				ImGui::PopStyleColor();

				ImGui::PopID();
				++it;
			}

			ImGui::EndTable();
		}

		ImGui::EndChild();
#endif
	}


	/// <summary>
	/// テクスチャブラウザ
	/// </summary>
#ifdef USE_IMGUI
	void GpuEmitManagerEditorUI::DrawTextureBrowser(GpuEmitManager& manager, bool& isOpen)
	{
		(void)isOpen;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
		// FileBrowser::Draw() が内部で Child を生成し、コールバックで選択通知する
		manager.textureBrowser_.Draw("TextureBrowser", ImVec2(0, 350));
		ImGui::PopStyleVar();
	}
#endif

	/// <summary>
	/// エディタータブ
	/// </summary>
	void GpuEmitManagerEditorUI::DrawEditorTab(GpuEmitManager& manager)
	{
#ifdef USE_IMGUI
		ImGui::BeginChild("Editor", ImVec2(0, 0), false);

		if (manager.selectedGroupName_.empty() || manager.selectedEmitterName_.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
				"⚠ エミッターを選択してください");
			ImGui::Text("「エミッター管理」タブでエミッターを選択してください。");
			ImGui::EndChild();
			return;
		}

		auto* emitterData = manager.GetEmitter(manager.selectedGroupName_, manager.selectedEmitterName_);
		if (!emitterData || !emitterData->emitter)
		{
			ImGui::EndChild();
			return;
		}

		// ヘッダー情報
		ImGui::SeparatorText(("編集中: " + emitterData->name).c_str());
		ImGui::TextDisabled("形状: %s", GpuEmitManager::shapeNames_[static_cast<int>(emitterData->shape)]);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// スクロール領域
		ImGui::BeginChild("EditorScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

		// パーティクルパラメータ編集
		if (DrawParticleParametersEditor(manager, emitterData)) {
			manager.UpdateParticleParams(emitterData);
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// エミッター形状パラメータ編集
		if (ImGui::CollapsingHeader("エミッター形状設定", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// 形状変更
			int currentShape = static_cast<int>(emitterData->shape);
			if (ImGui::Combo("形状", &currentShape, GpuEmitManager::shapeNames_, (static_cast<int>(EmitterShape::Mesh) + 1)))
			{
				emitterData->shape = static_cast<EmitterShape>(currentShape);
				emitterData->emitter->SetEmitterShape(emitterData->shape);
				manager.UpdateEmitterParams(emitterData);
			}

			ImGui::Spacing();

			// 形状別パラメータ
			if (DrawShapeEditor(manager, emitterData)) {
				manager.UpdateEmitterParams(emitterData);
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 統計情報
		if (ImGui::CollapsingHeader("パーティクル統計情報"))
		{
			auto stats = emitterData->emitter->GetGPUParticle()->GetCachedStats();

			if (stats.isValid) {
				ImGui::Text("アクティブ数: %u / %u", stats.activeCount, stats.maxParticles);
				ImGui::Text("未使用スロット数: %u", stats.freeCount);

				ImGui::ProgressBar(
					stats.usagePercent / 100.0f,
					ImVec2(-1, 0),
					std::format("{:.1f}%%", stats.usagePercent).c_str()
				);

				if (stats.freeListIndex < 0) {
					ImGui::TextColored(ImVec4(1, 0, 0, 1),
						"エラー: 空きパーティクルがありません！");
				}
			} else {
				ImGui::TextColored(ImVec4(1, 1, 0, 1),
					"統計情報を読み込み中...");
			}

			if (ImGui::Button("詳細統計を表示", ImVec2(-1, 0))) {
				emitterData->emitter->GetGPUParticle()->DrawStatsImGui();
			}
		}

		ImGui::EndChild();

		ImGui::EndChild();
#endif
	}
	/// <summary>
	/// 削除確認ダイアログ
	/// </summary>
	void GpuEmitManagerEditorUI::DrawDeleteDialog(GpuEmitManager& manager)
	{
#ifdef USE_IMGUI
		if (manager.showDeleteDialog_) {
			ImGui::OpenPopup("削除確認");
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("削除確認", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			// 削除対象によってメッセージを変更
			if (manager.selectedGroupName_.empty()) {
				ImGui::Text("すべてのエミッターグループを削除しますか？");
				ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "この操作は取り消せません！");
			} else {
				ImGui::Text("選択中のグループ '%s' を削除しますか？", manager.selectedGroupName_.c_str());
				ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "グループ内の全エミッターも削除されます！");
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// 削除実行ボタン
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("削除する", ImVec2(120, 0))) {
				if (manager.selectedGroupName_.empty()) {
					manager.DeleteAllEmitterGroups();
				} else {
					manager.DeleteEmitterGroup(manager.selectedGroupName_);
				}

				manager.showDeleteDialog_ = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopStyleColor();

			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();

			if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
				manager.showDeleteDialog_ = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
#endif
	}

} // namespace YoRigine

#endif
