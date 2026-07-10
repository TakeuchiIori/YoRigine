#include "GpuEmitManager.h"

// C++
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>

// Engine
#include <Loaders/Json/ConversionJson.h>
#include <Loaders/Texture/TextureManager.h>
#include <ModelManager.h>
#include <Debugger/Logger.h>
#include "Systems/GameTime/GameTime.h"
#include <IconsFontAwesome5.h>

namespace YoRigine {
	// ImGui用の形状名一覧
	const char* GpuEmitManager::shapeNames_[] = {
		"円形",
		"箱形",
		"三角形",
		"コーン",
		"メッシュ"
	};

	/// <summary>
	/// GpuEmitManager シングルトン取得
	/// </summary>
	GpuEmitManager* GpuEmitManager::GetInstance()
	{
		static GpuEmitManager instance;
		return &instance;
	}

	/// <summary>
	/// 初期化（カメラ登録のみ）
	/// </summary>
	void GpuEmitManager::Initialize()
	{
#ifdef USE_IMGUI
		// --- テクスチャブラウザ ---
		textureBrowser_ = FileBrowser(
			"Resources/Textures/",
			{ ".png", ".jpg", ".dds" },
			FileBrowser::DisplayMode::Grid
		);
		// サムネイルプロバイダ: TextureManager 経由で GPU ハンドルを返す
		textureBrowser_.SetThumbnailProvider([](const std::string& path) -> ImTextureID {
			TextureManager::GetInstance()->LoadTexture(path);
			auto handle = TextureManager::GetInstance()->GetsrvHandleGPU(path);
			return handle.ptr != 0 ? static_cast<ImTextureID>(handle.ptr) : 0;
			});
		// 選択時コールバック: テクスチャパスを入力バッファに反映
		textureBrowser_.SetOnFileSelected([this](const std::string& path) {
			strncpy_s(newEmitterTexturePath_, path.c_str(), sizeof(newEmitterTexturePath_) - 1);
			showTextureBrowser_ = false;
			});

		// --- JSON ブラウザ ---
		jsonBrowser_ = FileBrowser(
			"Resources/Json/GpuEmitters/",
			{ ".json" },
			FileBrowser::DisplayMode::List
		);
		// 選択時コールバック: パスをセーブ/ロードバッファに反映
		jsonBrowser_.SetOnFileSelected([this](const std::string& path) {
			strncpy_s(saveFilePath_, path.c_str(), sizeof(saveFilePath_) - 1);
			selectedJsonFilePath_ = path;
			selectedGroupName_.clear();
			selectedEmitterName_.clear();
			newGroupName_[0] = '\0';
			});

		// エミッタ形状のライン可視化（Debugのみ）
		gizmoLine_ = std::make_unique<Line>();
		gizmoLine_->Initialize();
		gizmoLine_->SetCamera(camera_);
#endif

		ModelManager::GetInstance()->LoadModel("Resources/Models/Cube", "Cube.obj");
		LoadAllEmitters();
	}

	/// <summary>
	/// 全エミッターを更新 (グループ内をループ)
	/// </summary>
	void GpuEmitManager::Update()
	{
		float deltaTime = GameTime::GetDeltaTime(TimeChannel::Vfx);

		// グループ全体をループ
		for (auto& [groupName, groupData] : groups_) {
			if (!groupData->isActive) continue;

			// 発生中(isPlaying) か、発生停止後の余韻(lingerTimer>0)がある間は粒子シミュレーションを回す。
			// これにより「停止後も既存粒子が自然に消える」「発生中でなくても撃ったワンショットが動く」が成立する。
			const bool emitting = groupData->isPlaying;
			const bool simulating = emitting || groupData->lingerTimer > 0.0f;
			if (!simulating) continue;

			// システム時間の更新
			if (emitting) {
				groupData->currentTime += deltaTime;
			} else {
				// 余韻中はエミッションせず、寿命ぶんだけ回して自然消滅を待つ
				groupData->lingerTimer -= deltaTime;
			}

			// グループ内のエミッターをループ
			for (auto& [emitterName, emitterData] : groupData->emitters) {
				if (emitterData->isActive && emitterData->emitter) {
					// 継続発生は再生中のみ。停止/余韻中はバースト要求だけが発生する
					emitterData->emitter->SetContinuousEmit(emitting);
					// グループ原点＋ローカルオフセットをワールド発生位置として毎フレーム反映（追従）
					emitterData->emitter->SetEmitWorldPosition(
						groupData->translate + GetEmitterLocalOffset(emitterData.get()));
					emitterData->emitter->SetTrailParams(emitterData->trailParams);
					emitterData->emitter->Update(deltaTime);
				}
			}

			// システムの自動終了判定 (Durationが0より大きく、再生時間がDurationを超えたら停止)
			if (emitting && groupData->systemDuration > 0.0f && groupData->currentTime >= groupData->systemDuration) {
				StopEmitterGroup(groupName);
			}
		}
	}

	/// <summary>
	/// 全エミッター描画 (グループ内をループ)
	/// </summary>
	void GpuEmitManager::Draw()
	{
		for (auto& [groupName, groupData] : groups_) {
			if (groupData->isActive) {
				for (auto& [emitterName, emitterData] : groupData->emitters) {
					if (emitterData->isActive && emitterData->emitter) {
						emitterData->emitter->Draw();
					}
				}
			}
		}

#ifdef USE_IMGUI
		// エミッタ形状のライン可視化（選択中グループ）
		if (showEmitterGizmos_ && gizmoLine_) {
			DrawEmitterGizmos();
		}
#endif
	}

#ifdef USE_IMGUI
	/// <summary>
	/// 選択中グループの各エミッタ形状をライン描画（選択エミッタ=黄 / それ以外=シアン）
	/// </summary>
	void GpuEmitManager::DrawEmitterGizmos()
	{
		gizmoLine_->Reset();

		EmitterGroup* group = GetGroup(selectedGroupName_);
		if (!group) return;

		// --- 非選択エミッタ（シアン）---
		for (auto& [name, e] : group->emitters) {
			if (!e || name == selectedEmitterName_) continue;
			RegisterEmitterGizmo(e.get(), group->translate + GetEmitterLocalOffset(e.get()));
		}
		gizmoLine_->SetColor({ 0.2f, 0.8f, 1.0f, 1.0f });
		gizmoLine_->DrawLine();

		// --- 選択エミッタ（黄・強調）---
		auto it = group->emitters.find(selectedEmitterName_);
		if (it != group->emitters.end() && it->second) {
			RegisterEmitterGizmo(it->second.get(), group->translate + GetEmitterLocalOffset(it->second.get()));
			gizmoLine_->SetColor({ 1.0f, 0.9f, 0.1f, 1.0f });
			gizmoLine_->DrawLine();
		}
	}

	/// <summary>
	/// 単一エミッタの形状を gizmoLine_ に線分登録（DrawLine は呼び出し側が発行）
	/// </summary>
	void GpuEmitManager::RegisterEmitterGizmo(const EmitterData* e, const Vector3& worldPos)
	{
		if (!e) return;

		switch (e->shape) {
		case EmitterShape::Sphere:
			gizmoLine_->DrawSphere(worldPos, e->sphereParams.radius, 16);
			break;

		case EmitterShape::Box:
		{
			// size はフルサイズ（shader は ±size*0.5 で分布）
			Vector3 half = e->boxParams.size * 0.5f;
			gizmoLine_->DrawAABB(worldPos - half, worldPos + half);
			break;
		}

		case EmitterShape::Cone:
		{
			const auto& cp = e->coneParams;
			const Vector3 apex = worldPos;

			// 方向を正規化
			Vector3 d = cp.direction;
			float dlen = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
			d = (dlen < 1e-4f) ? Vector3{ 0.0f, 1.0f, 0.0f } : d * (1.0f / dlen);

			const Vector3 baseC = apex + d * cp.height;

			// 方向に垂直な基底 (r, u) を作る
			Vector3 up = (std::fabs(d.y) < 0.999f) ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
			Vector3 r = { up.y * d.z - up.z * d.y, up.z * d.x - up.x * d.z, up.x * d.y - up.y * d.x };
			float rlen = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
			if (rlen > 1e-4f) r = r * (1.0f / rlen);
			Vector3 u = { d.y * r.z - d.z * r.y, d.z * r.x - d.x * r.z, d.x * r.y - d.y * r.x };

			const int seg = 16;
			const float twoPi = 6.28318530718f;
			Vector3 prev{};
			for (int i = 0; i <= seg; ++i) {
				float a = twoPi * float(i) / float(seg);
				Vector3 p = baseC + (r * std::cos(a) + u * std::sin(a)) * cp.radius;
				if (i > 0) gizmoLine_->RegisterLine(prev, p);
				prev = p;
				if (i % 4 == 0) gizmoLine_->RegisterLine(apex, p); // 側面のガイド線
			}
			break;
		}

		case EmitterShape::Triangle:
		{
			// shader は translate を使わず v1/v2/v3 を直接使うため、グループ原点だけを基準にする
			const Vector3 base = worldPos - e->triangleParams.translate;
			const auto& tp = e->triangleParams;
			gizmoLine_->RegisterLine(base + tp.v1, base + tp.v2);
			gizmoLine_->RegisterLine(base + tp.v2, base + tp.v3);
			gizmoLine_->RegisterLine(base + tp.v3, base + tp.v1);
			break;
		}

		case EmitterShape::Mesh:
			// メッシュ境界の取得は重いので、発生位置に小さなマーカー球のみ
			gizmoLine_->DrawSphere(worldPos, 0.5f, 8);
			break;
		}
	}
#endif
	
	/// <summary>
	/// エミッターのパラメータを更新
	/// </summary>
	/// <param name="emitterData"></param>
	void GpuEmitManager::UpdateParticleParams(EmitterData* emitterData)
	{
		if (!emitterData || !emitterData->emitter) return;

		auto* emitter = emitterData->emitter.get();
		auto& params = emitterData->particleParams;
		emitter->SetParticleParameters(params);
	}

	/// <summary>
	/// エミッションを強制発生
	/// </summary>
	/// <param name="groupName"></param>
	/// <param name="position"></param>
	/// <param name="count"></param>
	void GpuEmitManager::EmitGroups(const std::string& groupName, const Vector3& position, float count)
	{
		// グループを検索
		auto groupIt = groups_.find(groupName);
		if (groupIt == groups_.end()) {
			Logger("GpuEmitManager::EmitGroups() : エミッターグループが見つかりません");
			return;
		}

		// グループ内の全エミッターに対してエミッションを発生
		// 各エミッタは指定位置＋自身のローカルオフセットで発生（グループ内の相対配置を保持）
		auto& groupData = groupIt->second;
		groupData->translate = position;
		for (auto& [emitterName, emitterData] : groupData->emitters) {
			if (emitterData->isActive && emitterData->emitter) {
				// count < 0 は「各エミッタが持つ設定値を使う」の意味
				float emitCount = count;
				if (emitCount < 0.0f) {
					switch (emitterData->shape) {
					case EmitterShape::Sphere:   emitCount = emitterData->sphereParams.count;   break;
					case EmitterShape::Box:      emitCount = emitterData->boxParams.count;      break;
					case EmitterShape::Triangle: emitCount = emitterData->triangleParams.count; break;
					case EmitterShape::Cone:     emitCount = emitterData->coneParams.count;     break;
					case EmitterShape::Mesh:     emitCount = emitterData->meshParams.count;     break;
					}
				}
				emitterData->emitter->EmitAtPosition(position + GetEmitterLocalOffset(emitterData.get()), emitCount);
			}
		}
		// ワンショットは isPlaying を立てず、粒子が寿命を全うするまで linger でシミュレーションを継続させる
		groupData->lingerTimer = std::max(groupData->lingerTimer, GetGroupMaxLifetime(groupData.get()));
	}

	/// <summary>
	/// グループ内の最大パーティクル寿命を取得（発生停止後の linger 時間見積り用）
	/// </summary>
	float GpuEmitManager::GetGroupMaxLifetime(const EmitterGroup* group) const
	{
		float maxLife = 0.5f; // 最低保証（空グループ等でも最小限ティックする）
		if (!group) return maxLife;
		for (const auto& [name, e] : group->emitters) {
			if (!e) continue;
			const float life = e->particleParams.lifeTime + e->particleParams.lifeTimeVariance;
			maxLife = std::max(maxLife, life);
			// パーティクル自身のトレイル寿命も考慮
			if (e->particleParams.child.isTrail) {
				maxLife = std::max(maxLife, life + e->particleParams.child.lifeTime);
			}
		}
		return maxLife;
	}

	/// <summary>
	/// エミッターの形状ごとのローカル発生位置（オフセット）を取得
	/// </summary>
	Vector3 GpuEmitManager::GetEmitterLocalOffset(const EmitterData* emitterData) const
	{
		if (!emitterData) return { 0.0f, 0.0f, 0.0f };
		switch (emitterData->shape) {
		case EmitterShape::Sphere:   return emitterData->sphereParams.translate;
		case EmitterShape::Box:      return emitterData->boxParams.translate;
		case EmitterShape::Triangle: return emitterData->triangleParams.translate;
		case EmitterShape::Cone:     return emitterData->coneParams.translate;
		case EmitterShape::Mesh:     return emitterData->meshParams.translate;
		}
		return { 0.0f, 0.0f, 0.0f };
	}

	/// <summary>
	/// グループのワールド原点を移動
	/// </summary>
	void GpuEmitManager::SetGroupPosition(const std::string& groupName, const Vector3& pos)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (group) group->translate = pos;
	}

	/// <summary>
	/// グループが存在するか
	/// </summary>
	bool GpuEmitManager::HasGroup(const std::string& groupName) const
	{
		return groups_.count(groupName) > 0;
	}

	/// <summary>
	/// グループが再生中か
	/// </summary>
	bool GpuEmitManager::IsGroupPlaying(const std::string& groupName) const
	{
		auto it = groups_.find(groupName);
		return it != groups_.end() && it->second->isPlaying;
	}
	/// <summary>
	/// エミッター管理用の ImGui 描画
	/// </summary>
	void GpuEmitManager::DrawImGui()
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
				if (SaveToFile(saveFilePath_))
					std::cout << "保存成功: " << saveFilePath_ << std::endl;
				else
					std::cout << "保存失敗: " << saveFilePath_ << std::endl;
			}
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_FOLDER_OPEN " 読み込み", ImVec2(halfWidth, 0))) {
				if (LoadFromFile(saveFilePath_))
					std::cout << "読み込み成功: " << saveFilePath_ << std::endl;
				else
					std::cout << "読み込み失敗: " << saveFilePath_ << std::endl;
			}

			ImGui::Separator();

			// 2. パス入力とディレクトリのスキャンロジック
			ImGui::InputText("ファイルパス", saveFilePath_, sizeof(saveFilePath_));

			// 編集中のパスからディレクトリ部分を抽出 (既存ロジック)
			std::filesystem::path currentPath(saveFilePath_);

			// ディレクトリが変わっていたら再スキャン
			std::string dirPath = currentPath.has_filename()
				? currentPath.parent_path().string()
				: currentPath.string();
			if (!dirPath.empty() && dirPath.back() != '/') dirPath += '/';

			if (dirPath != jsonBrowser_.GetCurrentDir()) {
				jsonBrowser_.Scan(dirPath);
			}

			// FileBrowser に全部任せる
			jsonBrowser_.Draw("JsonList", ImVec2(0, 150));

			ImGui::Separator();

			// 全削除ボタン（目立つように配置）
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // 危険な操作なので赤くする
			if (ImGui::Button("全削除", ImVec2(ImGui::GetContentRegionAvail().x, 0)) && !groups_.empty()) {
				showDeleteDialog_ = true;
				selectedGroupName_.clear();
			}
			ImGui::PopStyleColor();
		}
		// エミッタ形状のライン可視化トグル
		ImGui::Checkbox("エミッタ形状をライン表示（選択グループ）", &showEmitterGizmos_);
		ImGui::SameLine();
		ImGui::TextDisabled("(黄=選択エミッタ / 水色=同グループの他)");

		// タブバーでセクション分け
		if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_None))
		{
			// ===== グループ管理タブ =====
			if (ImGui::BeginTabItem("グループ管理"))
			{
				DrawGroupManagementTab();
				ImGui::EndTabItem();
			}

			// ===== エミッター管理タブ =====
			if (ImGui::BeginTabItem("エミッター管理"))
			{
				DrawEmitterManagementTab();
				ImGui::EndTabItem();
			}

			// ===== エディタータブ =====
			if (ImGui::BeginTabItem("エディター"))
			{
				DrawEditorTab();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		// 削除確認ダイアログ
		DrawDeleteDialog();

#endif // USE_IMGUI
	}

	bool GpuEmitManager::DrawParticleParametersEditor(EmitterData* emitterData)
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
			if (ImGui::CollapsingHeader("トレイル", ImGuiTreeNodeFlags_None))
			{
				ImGui::PopStyleColor();
				if (ImGui::CollapsingHeader("エミッター自身のトレイル設定")) {
					changed |= ImGui::Checkbox("有効化", &emitterData->trailParams.isTrail);
					changed |= ImGui::Checkbox("エミッターのスケールを継承", &emitterData->trailParams.inheritScale);
					ImGui::DragFloat("トレイル生成距離", &emitterData->trailParams.minDistance, 0.01f, 0.0f, 1000.0f);
					ImGui::DragFloat("トレイル寿命", &emitterData->trailParams.lifeTime, 0.01f, 0.0f, 1000.0f);
					ImGui::DragFloat("生成パーティクル数", &emitterData->trailParams.emissionCount, 1.0f, 1.0f, 100000.0f);
				}

				if (ImGui::CollapsingHeader("パーティクル自身のトレイル設定")) {
					changed |= ImGui::Checkbox("有効化",&emitterData->particleParams.child.isTrail);
					changed |= ImGui::Checkbox("親のスケールを継承", &emitterData->particleParams.child.isInheritScale);
					ImGui::DragFloat("寿命", &emitterData->particleParams.child.lifeTime, 0.1f, 5.0f);
					ImGui::DragFloat("生成距離", &emitterData->particleParams.child.minDistance, 0.01f, 0.01f, 1000.0f);
					ImGui::DragFloat("開始スケール", &emitterData->particleParams.child.startScale, 0.01f, 0.01f, 100.0f);
					ImGui::DragFloat("終了スケール", &emitterData->particleParams.child.endScale, 0.01f, 0.0f, 100.0f);
					ImGui::DragInt("パーティクル生成数",&emitterData->particleParams.child.emissionCount, 1.0f, 10000);
				}
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

		return changed;
#else
		(void)emitterData;
		return false;
#endif
	}

	/// <summary>
	/// 現在の形状に応じたパラメータ編集を表示
	/// </summary>
	bool GpuEmitManager::DrawShapeEditor(EmitterData* emitterData)
	{
		switch (emitterData->shape)
		{
		case EmitterShape::Sphere:   return DrawSphereEditor(emitterData);
		case EmitterShape::Box:      return DrawBoxEditor(emitterData);
		case EmitterShape::Triangle: return DrawTriangleEditor(emitterData);
		case EmitterShape::Cone:     return DrawConeEditor(emitterData);
		case EmitterShape::Mesh:     return DrawMeshEditor(emitterData);
		}
		return false;
	}

	/// <summary>
	/// Sphere パラメータの ImGui 編集
	/// </summary>
	bool GpuEmitManager::DrawSphereEditor(EmitterData* emitterData)
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
	bool GpuEmitManager::DrawBoxEditor(EmitterData* emitterData)
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
	bool GpuEmitManager::DrawTriangleEditor(EmitterData* emitterData)
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
	bool GpuEmitManager::DrawConeEditor(EmitterData* emitterData)
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

	bool GpuEmitManager::DrawMeshEditor(EmitterData* emitterData)
	{
		(void)emitterData;
#ifdef USE_IMGUI
		bool changed = false;
		auto& p = emitterData->meshParams;

		// -------------------------
		// モデル選択コンボボックス
		// -------------------------
		auto modelKeys = ModelManager::GetInstance()->GetModelKeys();
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
					p.model = ModelManager::GetInstance()->FindModel(modelKeys[i]);
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
	void GpuEmitManager::DrawGroupManagementTab()
	{
#ifdef USE_IMGUI
		ImGui::BeginChild("GroupManagement", ImVec2(0, 0), false);

		// ===== 新規グループ作成 =====
		ImGui::SeparatorText("新規グループ作成");
		ImGui::TextDisabled("名前を入れて「＋作成」だけ。保存先は名前から自動決定（JSONの事前選択は不要）。");

		ImGui::PushItemWidth(-150);
		ImGui::InputTextWithHint("##NewGroupName", "グループ名を入力...", newGroupName_, sizeof(newGroupName_));
		ImGui::PopItemWidth();

		ImGui::SameLine();
		ImGui::BeginDisabled(strlen(newGroupName_) == 0);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.6f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.35f, 1.0f));
		if (ImGui::Button(ICON_FA_PLUS " 作成", ImVec2(140, 0))) {
			if (CreateEmitterGroup(newGroupName_)) {
				selectedGroupName_ = newGroupName_;
			}
			newGroupName_[0] = '\0';
		}
		ImGui::PopStyleColor(2);
		ImGui::EndDisabled();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// ===== グループリスト =====
		ImGui::SeparatorText("グループリスト");

		ImGui::Text("登録グループ数: %zu", groups_.size());

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

			for (auto& [name, groupData] : groups_)
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
				bool isSelected = (selectedGroupName_ == name);

				ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns;
				if (ImGui::Selectable(name.c_str(), isSelected, flags)) {
					selectedGroupName_ = name;
					selectedEmitterName_.clear();
				}

				// 右クリックメニュー
				if (ImGui::BeginPopupContextItem())
				{
					if (ImGui::MenuItem("削除")) {
						DeleteEmitterGroup(name);
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
						StopEmitterGroup(name);
					}
					ImGui::PopStyleColor();
				} else {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
					if (ImGui::SmallButton(ICON_FA_PLAY)) {
						PlayEmitterGroup(name);
					}
					ImGui::PopStyleColor();
				}
				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		ImGui::Spacing();

		// ===== 選択グループの詳細 =====
		EmitterGroup* currentGroup = GetGroup(selectedGroupName_);
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
						StopEmitterGroup(selectedGroupName_);
					}
				} else {
					ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "○ 停止中");
					ImGui::SameLine();
					if (ImGui::Button(ICON_FA_PLAY " 再生")) {
						PlayEmitterGroup(selectedGroupName_);
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
				SaveGroupToFile(selectedGroupName_);
			}
			ImGui::PopStyleColor(2);

			ImGui::Spacing();

			// グループ操作ボタン
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 0.8f));
			if (ImGui::Button("このグループを削除", ImVec2(-1, 0))) {
				showDeleteDialog_ = true;
			}
			ImGui::PopStyleColor();
		}

		ImGui::EndChild();
#endif
	}

	/// <summary>
	/// エミッター管理タブ
	/// </summary>
	void GpuEmitManager::DrawEmitterManagementTab()
	{
#ifdef USE_IMGUI
		ImGui::BeginChild("EmitterManagement", ImVec2(0, 0), false);

		if (selectedGroupName_.empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
				"⚠ グループを選択してください");
			ImGui::Text("「グループ管理」タブでグループを作成・選択してください。");
			ImGui::EndChild();
			return;
		}

		EmitterGroup* currentGroup = GetGroup(selectedGroupName_);
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
			newEmitterName_, sizeof(newEmitterName_));
		ImGui::PopItemWidth();

		// 形状選択
		ImGui::Text("形状:");
		ImGui::SameLine();
		ImGui::PushItemWidth(150);
		ImGui::Combo("##Shape", &selectedShapeIndex_, shapeNames_, IM_ARRAYSIZE(shapeNames_));
		ImGui::PopItemWidth();

		// テクスチャパス
		ImGui::Text("テクスチャ:");
		ImGui::PushItemWidth(-150);
		ImGui::InputTextWithHint("##TexturePath", "テクスチャパス...",
			newEmitterTexturePath_, sizeof(newEmitterTexturePath_));
		ImGui::PopItemWidth();

		ImGui::SameLine();
		static bool textureBrowserOpen = false;
		if (ImGui::Button("参照...", ImVec2(140, 0))) {
			ScanTextureDirectory("Resources/Textures/");
			textureBrowserOpen = !textureBrowserOpen;
		}

		// テクスチャブラウザ
		if (textureBrowserOpen)
		{
			ImGui::Spacing();
			DrawTextureBrowser(textureBrowserOpen);
		}

		ImGui::Spacing();

		// 作成ボタン
		ImGui::BeginDisabled(strlen(newEmitterName_) == 0);
		if (ImGui::Button("エミッター作成", ImVec2(-1, 35)))
		{
			std::string name = newEmitterName_;
			std::string texPath = newEmitterTexturePath_;
			EmitterShape shape = static_cast<EmitterShape>(selectedShapeIndex_);

			if (CreateEmitter(selectedGroupName_, name, texPath, shape))
			{
				selectedEmitterName_ = name;
				std::memset(newEmitterName_, 0, sizeof(newEmitterName_));
				std::memset(newEmitterTexturePath_, 0, sizeof(newEmitterTexturePath_));
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
				bool isSelected = (selectedEmitterName_ == name);

				if (ImGui::Selectable(name.c_str(), isSelected,
					ImGuiSelectableFlags_SpanAllColumns))
				{
					selectedEmitterName_ = name;
				}

				// 形状列
				ImGui::TableSetColumnIndex(2);
				ImGui::TextDisabled("%s", shapeNames_[static_cast<int>(data->shape)]);

				// 操作列
				ImGui::TableSetColumnIndex(3);
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 0.8f));
				if (ImGui::SmallButton("削除")) {
					if (selectedEmitterName_ == name) {
						selectedEmitterName_.clear();
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
	void GpuEmitManager::DrawTextureBrowser(bool& isOpen)
	{
		(void)isOpen;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
		// FileBrowser::Draw() が内部で Child を生成し、コールバックで選択通知する
		textureBrowser_.Draw("TextureBrowser", ImVec2(0, 350));
		ImGui::PopStyleVar();
	}
#endif

	/// <summary>
	/// エディタータブ
	/// </summary>
	void GpuEmitManager::DrawEditorTab()
	{
#ifdef USE_IMGUI
		ImGui::BeginChild("Editor", ImVec2(0, 0), false);

		if (selectedGroupName_.empty() || selectedEmitterName_.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
				"⚠ エミッターを選択してください");
			ImGui::Text("「エミッター管理」タブでエミッターを選択してください。");
			ImGui::EndChild();
			return;
		}

		auto* emitterData = GetEmitter(selectedGroupName_, selectedEmitterName_);
		if (!emitterData || !emitterData->emitter)
		{
			ImGui::EndChild();
			return;
		}

		// ヘッダー情報
		ImGui::SeparatorText(("編集中: " + emitterData->name).c_str());
		ImGui::TextDisabled("形状: %s", shapeNames_[static_cast<int>(emitterData->shape)]);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// スクロール領域
		ImGui::BeginChild("EditorScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

		// パーティクルパラメータ編集
		if (DrawParticleParametersEditor(emitterData)) {
			UpdateParticleParams(emitterData);
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// エミッター形状パラメータ編集
		if (ImGui::CollapsingHeader("エミッター形状設定", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// 形状変更
			int currentShape = static_cast<int>(emitterData->shape);
			if (ImGui::Combo("形状", &currentShape, shapeNames_, IM_ARRAYSIZE(shapeNames_)))
			{
				emitterData->shape = static_cast<EmitterShape>(currentShape);
				emitterData->emitter->SetEmitterShape(emitterData->shape);
				UpdateEmitterParams(emitterData);
			}

			ImGui::Spacing();

			// 形状別パラメータ
			if (DrawShapeEditor(emitterData)) {
				UpdateEmitterParams(emitterData);
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
	void GpuEmitManager::DrawDeleteDialog()
	{
#ifdef USE_IMGUI
		if (showDeleteDialog_) {
			ImGui::OpenPopup("削除確認");
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("削除確認", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			// 削除対象によってメッセージを変更
			if (selectedGroupName_.empty()) {
				ImGui::Text("すべてのエミッターグループを削除しますか？");
				ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "この操作は取り消せません！");
			} else {
				ImGui::Text("選択中のグループ '%s' を削除しますか？", selectedGroupName_.c_str());
				ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "グループ内の全エミッターも削除されます！");
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// 削除実行ボタン
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("削除する", ImVec2(120, 0))) {
				if (selectedGroupName_.empty()) {
					DeleteAllEmitterGroups();
				} else {
					DeleteEmitterGroup(selectedGroupName_);
				}

				showDeleteDialog_ = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopStyleColor();

			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();

			if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
				showDeleteDialog_ = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
#endif
	}



	/// <summary>
	/// 新しいエミッターを作成
	/// </summary>
	GpuEmitManager::EmitterData* GpuEmitManager::CreateEmitter(const std::string& groupName, const std::string& emitterName, std::string& texturePath, EmitterShape shape)
	{
		EmitterGroup* group = GetGroup(groupName);
		// グループが存在しない
		if (!group) return nullptr;

		// 同名のエミッターが存在する
		if (group->emitters.count(emitterName)) return nullptr;

		//-------------------- 値保存をしてから生成 --------------------//
		auto newEmitterData = std::make_unique<EmitterData>();
		newEmitterData->name = emitterName;
		newEmitterData->shape = shape;
		newEmitterData->isActive = true;
		newEmitterData->texturePath = texturePath;
		newEmitterData->emitter = std::make_unique<GPUEmitter>();

		// GPUEmitter の初期化
		newEmitterData->emitter->Initialize(camera_, texturePath);
		newEmitterData->emitter->SetEmitterShape(shape);

		// デフォルトパラメータ反映
		UpdateEmitterParams(newEmitterData.get());

		// グループのコンテナに追加
		group->emitters[emitterName] = std::move(newEmitterData);
		return group->emitters[emitterName].get();
	}

	/// <summary>
	/// エミッターを 1 つ削除
	/// </summary>
	void GpuEmitManager::DeleteEmitter(const std::string& groupName, const std::string& emitterName)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (!group) return;

		if (group->emitters.count(emitterName)) {
			group->emitters.erase(emitterName);
			if (selectedEmitterName_ == emitterName) {
				selectedEmitterName_.clear();
			}
		}
	}

	/// <summary>
	/// 全エミッター削除
	/// </summary>
	void GpuEmitManager::DeleteAllEmitters()
	{
		for (auto& [name, data] : groups_) {
			data->emitters.clear();
		}
	}

	GpuEmitManager::EmitterGroup* GpuEmitManager::GetGroup(const std::string& groupName)
	{
		if (groups_.count(groupName)) {
			return groups_.at(groupName).get();
		}
		return nullptr;
	}

	GpuEmitManager::EmitterGroup* GpuEmitManager::CreateEmitterGroup(const std::string& groupName, const std::string& sourceFilePath)
	{
		// 1. グループ名で重複がないかチェック（非致命: 既存を返さず nullptr。ログのみ）
		if (groups_.count(groupName)) {
			Logger("GpuEmitManager: グループ名 '" + groupName + "' は既に存在するため作成をスキップ");
			return nullptr;
		}

		// 2. 由来ファイルパスを決定（引数優先→エディタ選択中→最後はグループ名から自動決定）
		//    これによりエディタで JSON を手選択しなくても新規グループを作成・保存できる
		std::string source = !sourceFilePath.empty() ? sourceFilePath : selectedJsonFilePath_;
		if (source.empty()) {
			source = "Resources/Json/GpuEmitters/" + groupName + ".json";
		}

		// 3. 新しいグループを作成
		auto newGroup = std::make_unique<EmitterGroup>();
		newGroup->name = groupName;
		newGroup->sourceFilePath = source;

		// 4. groups_ に追加
		auto [it, inserted] = groups_.emplace(groupName, std::move(newGroup));
		if (inserted) {
			Logger("Group created: " + groupName + " (File: " + source + ")");
			return it->second.get();
		}

		return nullptr;
	}
	
	void GpuEmitManager::DeleteEmitterGroup(const std::string& groupName)
	{
		auto it = groups_.find(groupName);
		if (it == groups_.end()) {
			Logger("GpuEmitManager: 削除対象グループ '" + groupName + "' が見つかりません");
			return;
		}

		EmitterGroup* group = it->second.get();
		const std::string sourceFile = group->sourceFilePath;

		// グループ内の全エミッターを破棄
		group->emitters.clear();

		// グループを groups_ から削除
		groups_.erase(it);

		Logger("Group deleted: " + groupName + " (File: " + sourceFile + ")");

		// 選択状態のリセット
		if (selectedGroupName_ == groupName) {
			selectedGroupName_ = "";
			selectedEmitterName_ = "";
		}
	}

	void GpuEmitManager::DeleteAllEmitterGroups()
	{
		// グループコンテナ全体をクリア
		groups_.clear();

		// 選択中のグループ名とエミッター名をクリアしてリセット
		selectedGroupName_.clear();
		selectedEmitterName_.clear();
	}

	void GpuEmitManager::PlayEmitterGroup(const std::string& groupName)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (!group)return;

		// 再生中じゃない場合のみ処理する
		if (!group->isPlaying)
		{
			group->isPlaying = true;
			group->currentTime = 0.0f;
			group->lingerTimer = 0.0f;
			// グループ内のエミッターを再生（発生状態をリセットしてクリーンスタート）
			for (auto& [name, data] : group->emitters)
			{
				if (data->emitter) {
					data->emitter->Reset();
				}
			}
		}
	}

	void GpuEmitManager::StopEmitterGroup(const std::string& groupName)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (!group)return;

		if (!group->isPlaying) return;

		group->isPlaying = false;
		group->currentTime = 0.0f;

		// 停止時は Reset() で粒子を即消滅させず、linger でシミュレーションを続けて自然に消えるのを待つ
		// （剣トレイル等を Stop したとき既存粒子がプツッと消えるのを防ぐ）
		group->lingerTimer = std::max(group->lingerTimer, GetGroupMaxLifetime(group));
	}

	void GpuEmitManager::SetCamera(Camera* camera)
	{
		// GpuEmitManager 自身のカメラポインタを更新
		camera_ = camera;
#ifdef USE_IMGUI
		if (gizmoLine_) gizmoLine_->SetCamera(camera);
#endif
		// 全グループをループ
		for (auto& [groupName, groupData] : groups_) {
			for (auto& [emitterName, emitterDataPtr] : groupData->emitters) {
				if (emitterDataPtr) {
					GPUEmitter* emitter = emitterDataPtr->emitter.get();
					if (emitter) {
						emitter->SetCamera(camera);
					}
				}
			}
		}
	}

	/// <summary>
	/// エミッター名からデータ取得
	/// </summary>
	GpuEmitManager::EmitterData* GpuEmitManager::GetEmitter(const std::string& groupName, const std::string& emitterName)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (!group) return nullptr;

		if (group->emitters.count(emitterName)) {
			return group->emitters.at(emitterName).get();
		}
		return nullptr;
	}

	/// <summary>
	/// 全エミッター名リスト取得
	/// </summary>
	std::vector<std::string> GpuEmitManager::GetEmitterNames() const
	{
		std::vector<std::string> names;
		for (const auto& [groupName, groupPtr] : groups_) {
			for (const auto& [emitterName, _] : groupPtr->emitters) {
				names.push_back(emitterName);
			}
		}
		return names;
	}

	/// <summary>
	/// ロード済みグループ名の一覧
	/// </summary>
	std::vector<std::string> GpuEmitManager::GetGroupNames() const
	{
		std::vector<std::string> names;
		names.reserve(groups_.size());
		for (const auto& [name, group] : groups_) {
			names.push_back(name);
		}
		return names;
	}

	/// <summary>
	/// 指定エミッターが存在するか
	/// </summary>
	bool GpuEmitManager::HasEmitter(const std::string& emitterName) const
	{
		for (const auto& [groupName, groupPtr] : groups_) {
			if (groupPtr->emitters.count(emitterName)) {
				return true;
			}
		}
		return false;
	}

	/// <summary>
	/// エミッターの形状パラメータを GPUEmitter に反映
	/// </summary>
	void GpuEmitManager::UpdateEmitterParams(EmitterData* emitterData)
	{
		if (!emitterData || !emitterData->emitter) {
			return;
		}

		switch (emitterData->shape)
		{
		case EmitterShape::Sphere:
		{
			const auto& p = emitterData->sphereParams;
			emitterData->emitter->UpdateSphereParams(
				p.translate, p.radius, p.count, p.emitInterval
			);
		}
		break;

		case EmitterShape::Box:
		{
			const auto& p = emitterData->boxParams;
			emitterData->emitter->UpdateBoxParams(
				p.translate, p.size, p.count, p.emitInterval
			);
		}
		break;

		case EmitterShape::Triangle:
		{
			const auto& p = emitterData->triangleParams;
			emitterData->emitter->UpdateTriangleParams(
				p.v1, p.v2, p.v3, p.translate, p.count, p.emitInterval
			);
		}
		break;

		case EmitterShape::Cone:
		{
			const auto& p = emitterData->coneParams;
			emitterData->emitter->UpdateConeParams(
				p.translate, p.direction, p.radius, p.height, p.count, p.emitInterval
			);
		}
		break;
		case EmitterShape::Mesh:
		{
			const auto& p = emitterData->meshParams;
			emitterData->emitter->UpdateMeshParams(
				p.model, p.translate, p.scale, p.rotation,
				p.count, p.emitInterval, p.emitMode
			);
		}
		break;
		}
	}

	/// <summary>
	/// JSON ファイルとしてエミッター情報を保存
	/// </summary>
	bool GpuEmitManager::SaveToFile(const std::string& filepath)
	{
		try
		{
			nlohmann::json json = ToJson();

			// 必要ならディレクトリ作成
			std::filesystem::path path(filepath);
			std::filesystem::create_directories(path.parent_path());

			std::ofstream file(filepath);
			if (!file.is_open()) {
				return false;
			}

			file << json.dump(4);
			return true;
		}
		catch (const std::exception& e)
		{
			std::cerr << "Error saving emitters: " << e.what() << std::endl;
			return false;
		}
	}

	/// <summary>
	/// グループ単位で、そのグループ自身の sourceFilePath に保存する。
	/// 1ファイル1グループを基本とし、複数グループが1ファイルに混ざるのを防ぐ。
	/// </summary>
	bool GpuEmitManager::SaveGroupToFile(const std::string& groupName)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (!group) {
			Logger("GpuEmitManager::SaveGroupToFile : グループ '" + groupName + "' が見つかりません");
			return false;
		}

		// 保存先が未設定なら名前から自動決定
		if (group->sourceFilePath.empty()) {
			group->sourceFilePath = "Resources/Json/GpuEmitters/" + groupName + ".json";
		}

		try
		{
			nlohmann::json json;
			json["version"] = "1.0";
			json["groups"] = nlohmann::json::array();
			json["groups"].push_back(ToJsonGroup(group));

			std::filesystem::path path(group->sourceFilePath);
			if (path.has_parent_path()) {
				std::filesystem::create_directories(path.parent_path());
			}

			std::ofstream file(group->sourceFilePath);
			if (!file.is_open()) {
				Logger("GpuEmitManager::SaveGroupToFile : ファイルを開けません " + group->sourceFilePath);
				return false;
			}

			file << json.dump(4);
			Logger("GpuEmitManager: グループ '" + groupName + "' を保存 → " + group->sourceFilePath);
			return true;
		}
		catch (const std::exception& e)
		{
			std::cerr << "Error saving group: " << e.what() << std::endl;
			return false;
		}
	}

	/// <summary>
	/// JSON ファイルからエミッター情報を読み込み
	/// </summary>
	bool GpuEmitManager::LoadFromFile(const std::string& filepath)
	{
		try
		{
			std::ifstream file(filepath);

			if (!file.is_open()) {
				return false;
			}

			nlohmann::json json;
			file >> json;

			return FromJson(json, filepath);
		}
		catch (const std::exception& e)
		{
			std::cerr << "Error loading emitters: " << e.what() << std::endl;
			return false;
		}
	}


	/// <summary>
	/// ディレクトリ内全てを読み込む
	/// </summary>
	/// <param name="directory"></param>
	/// <returns></returns>
	bool GpuEmitManager::LoadAllEmitters(const std::string& directory)
	{
		ScanJsonDirectory(directory);
		bool allSucceeded = true; // 初期値は成功

		// スキャン結果のファイルパスをループ
		for (const auto& filepath : availableJsonFiles_) {

			// 各JSONファイルを個別に読み込み、エミッターとして登録
			if (LoadFromFile(directory + filepath)) {
				// 成功
				Logger("読み込み成功: " + filepath);
			}
			else {
				// 失敗
				Logger("読み込み失敗: " + filepath);
				allSucceeded = false; // 失敗フラグを立てる
			}
		}

		// 処理結果を返す
		return allSucceeded;
	}
	/// <summary>
	/// 全エミッターを JSON 化
	/// </summary>
	nlohmann::json GpuEmitManager::ToJson() const
	{
		nlohmann::json json;
		json["version"] = "1.0";
		json["groups"] = nlohmann::json::array();

		for (const auto& [groupName, groupPtr] : groups_)
		{
			json["groups"].push_back(ToJsonGroup(groupPtr.get()));
		}
		return json;
	}

	/// <summary>
	/// 単一グループを JSON 化（グループ単位保存・全体保存の両方から使う）
	/// </summary>
	nlohmann::json GpuEmitManager::ToJsonGroup(const EmitterGroup* groupPtr) const
	{
		nlohmann::json groupJson;
		if (!groupPtr) return groupJson;
		{
			// ------------------------------------------------------------
			// エミッターグループのパラメータをシリアライズ
			// ------------------------------------------------------------
			groupJson["groupName"] = groupPtr->name;
			groupJson["isActive"] = groupPtr->isActive;
			groupJson["isPlaying"] = groupPtr->isPlaying;
			groupJson["currentTime"] = groupPtr->currentTime;
			groupJson["systemDuration"] = groupPtr->systemDuration;
			groupJson["translate"] = groupPtr->translate;
			groupJson["emitters"] = nlohmann::json::array(); // グループ内のエミッター配列

			for (const auto& [emitterName, e] : groupPtr->emitters)
			{
				nlohmann::json j;
				//------------------------------------------------------------
				// 生成時に必要な情報
				//------------------------------------------------------------
				j["name"] = e->name;
				j["shape"] = static_cast<int>(e->shape);
				j["isActive"] = e->isActive;
				j["textureFilePath"] = e->texturePath;

				//------------------------------------------------------------
				// エミッターの形状
				//------------------------------------------------------------
				j["sphereParams"] = {
					{"translate", e->sphereParams.translate},
					{"radius",    e->sphereParams.radius},
					{"count",     e->sphereParams.count},
					{"emitInterval", e->sphereParams.emitInterval}
				};

				j["boxParams"] = {
					{"translate", e->boxParams.translate},
					{"size",      e->boxParams.size},
					{"count",     e->boxParams.count},
					{"emitInterval", e->boxParams.emitInterval}
				};

				j["triangleParams"] = {
					{"v1", e->triangleParams.v1},
					{"v2", e->triangleParams.v2},
					{"v3", e->triangleParams.v3},
					{"count", e->triangleParams.count},
					{"emitInterval", e->triangleParams.emitInterval}
				};

				j["coneParams"] = {
					{"translate", e->coneParams.translate},
					{"direction", e->coneParams.direction},
					{"radius",    e->coneParams.radius},
					{"height",    e->coneParams.height},
					{"count",     e->coneParams.count},
					{"emitInterval", e->coneParams.emitInterval}
				};
				j["meshParams"] = {
					{"modelName", e->meshParams.model ? e->meshParams.model->GetName() : ""},
					{"translate", e->meshParams.translate},
					{"scale", e->meshParams.scale},
					{"rotation", e->meshParams.rotation.x,e->meshParams.rotation.y,e->meshParams.rotation.z,e->meshParams.rotation.w},
					{"count", e->meshParams.count},
					{"emitInterval", e->meshParams.emitInterval},
					{"emitMode", static_cast<int>(e->meshParams.emitMode)}
				};



				//------------------------------------------------------------
				// パーティクルのパラメータ
				//------------------------------------------------------------
				j["particleParams"] = {
					{"lifeTime",    e->particleParams.lifeTime},
					{"lifeTimeVariance", e->particleParams.lifeTimeVariance},

					{"startScale",          e->particleParams.startScale},
					{"startScaleVariance",  e->particleParams.startScaleVariance},
					{"endScale",            e->particleParams.endScale},
					{"endScaleVariance",    e->particleParams.endScaleVariance},

					{"rotation",               e->particleParams.rotation},
					{"rotationVariance",       e->particleParams.rotationVariance},
					{"rotationSpeed",          e->particleParams.rotationSpeed},
					{"rotationSpeedVariance",  e->particleParams.rotationSpeedVariance},

					{"velocity",         e->particleParams.velocity},
					{"velocityVariance", e->particleParams.velocityVariance},

					{"startColor",         e->particleParams.startColor},
					{"startColorVariance", e->particleParams.startColorVariance},
					{"endColor",           e->particleParams.endColor},
					{"endColorVariance",   e->particleParams.endColorVariance},

					{"gravity",					e->particleParams.gravity},

					{"isBillboard",				e->particleParams.isBillboard},

					{"isTrail",					e->particleParams.child.isTrail},
					{"isInheritScale",			e->particleParams.child.isInheritScale},
					{"trailLifeTime",			e->particleParams.child.lifeTime},
					{"trailEmissionCount",		e->particleParams.child.emissionCount},
					{"trailMinDistance",		e->particleParams.child.minDistance},
					{"trailStartScale",			e->particleParams.child.startScale},
					{"trailEndScale",			e->particleParams.child.endScale},
				};

				j["trail"] = {
				{"enabled", e->trailParams.isTrail},
				{"minDistance",e->trailParams.minDistance},
				{"lifeTime",e->trailParams.lifeTime },
				{"emissionCount",e->trailParams.emissionCount },
				{"inheritScale", e->trailParams.inheritScale }
				};

				// 1粒子の描画メッシュ形状＋生成パラメータ
				j["particleMeshShape"] = static_cast<int>(e->particleMeshShape);
				j["particleMeshParams"] = {
					{"width",        e->particleMeshParams.width},
					{"height",       e->particleMeshParams.height},
					{"depth",        e->particleMeshParams.depth},
					{"outerRadius",  e->particleMeshParams.outerRadius},
					{"innerRadius",  e->particleMeshParams.innerRadius},
					{"radius",       e->particleMeshParams.radius},
					{"angleDegree",  e->particleMeshParams.angleDegree},
					{"divide",       e->particleMeshParams.divide},
					{"subdivisions", e->particleMeshParams.subdivisions},
				};

				groupJson["emitters"].push_back(j);
			}
		}
		return groupJson;
	}


	/// <summary>
	/// JSON からエミッター情報を復元
	/// </summary>
	bool GpuEmitManager::FromJson(const nlohmann::json& json, const std::string& sourceFilePath)
	{
		try
		{

			bool loaded = false;

			// 新しいグループ形式 ("groups") の読み込みを試みる
			if (json.contains("groups"))
			{
				// グループの配列をループ
				for (const auto& groupJson : json["groups"])
				{
					//------------------------------------------------------------
					// グループ情報の復元
					//------------------------------------------------------------
					// groupName がない場合に備えてデフォルト値 "LoadedGroup" を設定
					std::string groupName = groupJson.value("groupName", "LoadedGroup");

					// グループの作成（由来ファイルパスを明示的に渡す＝起動時ロードでも確実に作成される）
					EmitterGroup* group = CreateEmitterGroup(groupName, sourceFilePath);
					if (!group) continue;

					// グループパラメータの復元
					group->isActive = groupJson.value("isActive", true);
					group->isPlaying = groupJson.value("isPlaying", false);
					group->currentTime = groupJson.value("currentTime", 0.0f);
					group->systemDuration = groupJson.value("systemDuration", 0.0f);

					if (groupJson.contains("translate")) {
						group->translate = groupJson["translate"];
					}

					if (groupJson.contains("emitters")) // エミッター配列があればループ
					{
						for (const auto& j : groupJson["emitters"])
						{
							// 補助関数でエミッターをロード
							LoadEmitterFromJson(groupName, j);
						}
						loaded = true; // グループが存在し、処理が実行された
					}
				}
			}
			return loaded; // ロードできたかどうかを返す
		}
		catch (const std::exception& e)
		{
			// デシリアライズ中にエラーが発生した場合
			std::cerr << "Error parsing JSON: " << e.what() << std::endl;
			return false;
		}
	}

	/// <summary>
	/// JSONから単一のエミッター情報を復元
	/// </summary>
	bool GpuEmitManager::LoadEmitterFromJson(const std::string& groupName, const nlohmann::json& j)
	{
		//------------------------------------------------------------
		// 生成時に必要な情報
		//------------------------------------------------------------
		std::string		name = j.value("name", "UnnamedEmitter");
		EmitterShape	shape = static_cast<EmitterShape>(j.value("shape", 0)); // 0はSphereを想定
		bool			isActive = j.value("isActive", true);
		std::string texturePath = j.value("textureFilePath", "");

		if (CreateEmitter(groupName, name, texturePath, shape))
		{
			// GetEmitter に groupName を渡す
			auto* e = GetEmitter(groupName, name);
			if (!e) return false;

			e->isActive = isActive;

			//------------------------------------------------------------
			// エミッターの形状
			//------------------------------------------------------------
			// Sphere
			if (j.contains("sphereParams")) {
				const auto& p = j["sphereParams"];
				e->sphereParams.translate = p["translate"];
				e->sphereParams.radius = p["radius"];
				e->sphereParams.count = p["count"];
				e->sphereParams.emitInterval = p["emitInterval"];
			}

			// Box
			if (j.contains("boxParams")) {
				const auto& p = j["boxParams"];
				e->boxParams.translate = p["translate"];
				e->boxParams.size = p["size"];
				e->boxParams.count = p["count"];
				e->boxParams.emitInterval = p["emitInterval"];
			}

			// Triangle
			if (j.contains("triangleParams")) {
				const auto& p = j["triangleParams"];
				e->triangleParams.v1 = p["v1"];
				e->triangleParams.v2 = p["v2"];
				e->triangleParams.v3 = p["v3"];
				e->triangleParams.count = p["count"];
				e->triangleParams.emitInterval = p["emitInterval"];
			}

			// Cone
			if (j.contains("coneParams")) {
				const auto& p = j["coneParams"];
				e->coneParams.translate = p["translate"];
				e->coneParams.direction = p["direction"];
				e->coneParams.radius = p["radius"];
				e->coneParams.height = p["height"];
				e->coneParams.count = p["count"];
				e->coneParams.emitInterval = p["emitInterval"];
			}
			// Mesh
			if (j.contains("meshParams")) {
				const auto& mp = j["meshParams"];

				std::string modelName = mp.value("modelName", "");
				if (!modelName.empty()) {
					e->meshParams.model = ModelManager::GetInstance()->FindModel(modelName);
				}

				e->meshParams.translate = mp["translate"];
				e->meshParams.scale = mp["scale"];
				Vector4 r = mp["rotation"];
				e->meshParams.rotation = Quaternion(r.x, r.y, r.z, r.w);

				e->meshParams.count = mp["count"];
				e->meshParams.emitInterval = mp["emitInterval"];
				e->meshParams.emitMode = static_cast<MeshEmitMode>(mp["emitMode"]);
			}


			//------------------------------------------------------------
			// パーティクルのパラメータ
			//------------------------------------------------------------
			if (j.contains("particleParams"))
			{
				const auto& pp = j["particleParams"];
				e->particleParams.lifeTime = pp["lifeTime"];
				e->particleParams.lifeTimeVariance = pp["lifeTimeVariance"];
				e->particleParams.isBillboard = pp["isBillboard"];

				e->particleParams.startScale = pp["startScale"];
				e->particleParams.startScaleVariance = pp["startScaleVariance"];
				e->particleParams.endScale = pp["endScale"];
				e->particleParams.endScaleVariance = pp["endScaleVariance"];

				e->particleParams.rotation = pp["rotation"];
				e->particleParams.rotationVariance = pp["rotationVariance"];
				e->particleParams.rotationSpeed = pp["rotationSpeed"];
				e->particleParams.rotationSpeedVariance = pp["rotationSpeedVariance"];

				e->particleParams.velocity = pp["velocity"];
				e->particleParams.velocityVariance = pp["velocityVariance"];

				e->particleParams.startColor = pp["startColor"];
				e->particleParams.startColorVariance = pp["startColorVariance"];
				e->particleParams.endColor = pp["endColor"];
				e->particleParams.endColorVariance = pp["endColorVariance"];

				e->particleParams.gravity = pp["gravity"];

				// トレイルパラメータ
				e->particleParams.child.isTrail = pp["isTrail"];
				e->particleParams.child.isInheritScale = pp["isInheritScale"];
				e->particleParams.child.lifeTime = pp["trailLifeTime"];
				e->particleParams.child.emissionCount = pp["trailEmissionCount"];
				e->particleParams.child.minDistance = pp["trailMinDistance"];
				e->particleParams.child.startScale = pp["trailStartScale"];
				e->particleParams.child.endScale = pp["trailEndScale"];

			}
			//------------------------------------------------------------
			// トレイルのパラメータ
			//------------------------------------------------------------
			if (j.contains("trail"))
			{
				const auto& tp = j["trail"];
				e->trailParams.isTrail = tp.value("enabled", false);
				e->trailParams.minDistance = tp.value("minDistance", 0.1f);
				e->trailParams.lifeTime = tp.value("lifeTime", 1.0f);
				e->trailParams.emissionCount = tp.value("emissionCount", 1.0f);
				e->trailParams.inheritScale = tp.value("inheritScale", false);
			}

			//------------------------------------------------------------
			// 1粒子の描画メッシュ形状＋生成パラメータ（旧データには無いので既定）
			//------------------------------------------------------------
			e->particleMeshShape = static_cast<ParticleMeshShape>(
				j.value("particleMeshShape", static_cast<int>(ParticleMeshShape::Plane)));

			if (j.contains("particleMeshParams")) {
				const auto& mp = j["particleMeshParams"];
				auto& d = e->particleMeshParams;
				d.width = mp.value("width", d.width);
				d.height = mp.value("height", d.height);
				d.depth = mp.value("depth", d.depth);
				d.outerRadius = mp.value("outerRadius", d.outerRadius);
				d.innerRadius = mp.value("innerRadius", d.innerRadius);
				d.radius = mp.value("radius", d.radius);
				d.angleDegree = mp.value("angleDegree", d.angleDegree);
				d.divide = mp.value("divide", d.divide);
				d.subdivisions = mp.value("subdivisions", d.subdivisions);
			}

			// 適用
			UpdateEmitterParams(e);
			UpdateParticleParams(e);
			if (e->emitter) {
				e->emitter->SetParticleMesh(e->particleMeshShape, e->particleMeshParams);
			}

			return true;
		}
		return false;
	}
	// 画像一覧をスキャン
	void GpuEmitManager::ScanTextureDirectory(const std::string& directory)
	{
		currentTextureDir_ = directory;
		availableTextures_.clear();
		availableFolders_.clear();

		for (auto& p : std::filesystem::directory_iterator(directory))
		{
			if (p.is_directory()) {
				availableFolders_.push_back(p.path().filename().string());
				continue;
			}

			if (!p.is_regular_file()) continue;

			std::string ext = p.path().extension().string();

			std::transform(
				ext.begin(), ext.end(), ext.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); }
			);

			if (ext == ".png" || ext == ".jpg" || ext == ".dds")
			{
				availableTextures_.push_back(p.path().string());
			}
		}
	}
	// JSONファイル一覧をスキャン
	void GpuEmitManager::ScanJsonDirectory(const std::string& directory)
	{
		// ディレクトリが存在しない、またはディレクトリでない場合はクリアして終了
		if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
			availableJsonFiles_.clear();
			return;
		}

		currentJsonDir_ = directory;
		availableJsonFiles_.clear();

		for (auto& p : std::filesystem::directory_iterator(directory))
		{
			// ファイルであること
			if (!p.is_regular_file()) continue;

			// 拡張子が .json であること
			if (p.path().extension().string() == ".json")
			{
				availableJsonFiles_.push_back(p.path().filename().string());
			}
		}
	}
}
