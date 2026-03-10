#include "YParticleSystem.h"

#include <Loaders/Texture/TextureManager.h>

//------------------ モジュールのインクルード ------------------//
// Spawnモジュール
#include "Modules/Spawn/Color/SpawnRandomColor.h"

#include "Modules/Spawn/Life/SpawnLifeTime.h"

#include "Modules/Spawn/Movement/SpawnConeVelocity.h"
#include "Modules/Spawn/Movement/SpawnRadialVelocity.h"
#include "Modules/Spawn/Movement/SpawnVelocity.h"

#include "Modules/Spawn/Transform/SpawnRandomScale.h"
#include "Modules/Spawn/Transform/SpawnRandomRotate.h"

#include "Modules/Spawn/UV/SpawnUV.h"

// Updateモジュール
#include "Modules/Update/Physics/UpdateGravity.h"
#include "Modules/Update/Physics/UpdateAttractor.h"
#include "Modules/Update/Physics/UpdateCollision.h"
#include "Modules/Update/Physics/UpdateDrag.h"

#include "Modules/Update/Movement/UpdateOrbital.h"
#include "Modules/Update/Movement/UpdateSpiralConverge.h"
#include "Modules/Update/Movement/UpdateVelocity.h"

#include "Modules/Update/Color/UpdateColor.h"
#include "Modules/Update/Color/UpdateColorGradient.h"
#include "Modules/Update/Color/UpdateFadeOut.h"

#include "Modules/Update/Random/UpdateNoise.h"

#include "Modules/Update/Animation/UpdatePulse.h"

#include "Modules/Update/Transform/UpdateRotation.h"
#include "Modules/Update/Transform/UpdateScale.h"

#include "Modules/Update/UV/UpdateUVScroll.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif


YParticleSystem::YParticleSystem(const std::string& name, uint32_t maxParticles)
	: name_(name), maxParticles_(maxParticles)
{
#ifdef USE_IMGUI
	textureBrowser_ = YoRigine::FileBrowser(textureDirectory_, { ".png", ".jpg", ".dds" }, YoRigine::FileBrowser::DisplayMode::Grid);
	textureBrowser_.SetThumbnailProvider([](const std::string& path) -> ImTextureID {
		TextureManager::GetInstance()->LoadTexture(path);
		auto handle = TextureManager::GetInstance()->GetsrvHandleGPU(path);
		return handle.ptr != 0 ? static_cast<ImTextureID>(handle.ptr) : 0;
		});
	textureBrowser_.SetOnFileSelected([this](const std::string& path) {
		SetTexture(path);
		showTextureBrowser_ = false;
		});
#endif
	// 全パーティクル属性配列の初期化
	attributes_.resize(maxParticles_);

}

void YParticleSystem::AddSpawnModule(std::shared_ptr<ISpawnModule> module)
{
	if (module) {
		module->Initialize(maxParticles_);
		spawnModules_.push_back(module);
	}
}

void YParticleSystem::AddUpdateModule(std::shared_ptr<IUpdateModule> module)
{
	if (module) {
		module->Initialize(maxParticles_);
		updateModules_.push_back(module);
	}
}

void YParticleSystem::DeleteSpawnModule(const std::shared_ptr<ISpawnModule>& module) {
	spawnModules_.erase(
		std::remove(spawnModules_.begin(), spawnModules_.end(), module),
		spawnModules_.end()
	);
}

void YParticleSystem::DeleteUpdateModule(const std::shared_ptr<IUpdateModule>& module) {
	updateModules_.erase(
		std::remove(updateModules_.begin(), updateModules_.end(), module),
		updateModules_.end()
	);
}
void YParticleSystem::Update(float deltaTime)
{
	for (uint32_t i = 0; i < maxParticles_; ++i) {
		auto& attr = attributes_[i];
		if (!attr.isActive) continue;

		// 個別の経過時間を進める
		attr.currentTime += deltaTime;

		// 寿命チェック
		if (attr.currentTime >= attr.lifeTime) {
			attr.isActive = false;
			continue;
		}

		// モジュールの更新
		for (auto& module : updateModules_) {
			module->OnUpdate(attributes_.data(), i, deltaTime);
		}
	}
}

void YParticleSystem::Emit(const Vector3& position, int count)
{
	for (int i = 0; i < count; ++i) {
		int index = FindEmptyIndex();
		if (index < 0) break;

		// 状態のリセット
		attributes_[index].isActive = true;
		attributes_[index].position = position;
		attributes_[index].currentTime = 0.0f;
		attributes_[index].velocity = Vector3(0.0f, 0.0f, 0.0f);
		attributes_[index].scale = Vector3(1.0f, 1.0f, 1.0f);
		attributes_[index].rotation = Vector3(0.0f, 0.0f, 0.0f);
		attributes_[index].color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);


		// 登録された Spawn モジュールを適用して初期状態を決定
		for (auto& module : spawnModules_) {
			module->OnSpawn(attributes_.data(), index);
		}
	}
}

void YParticleSystem::SetTexture(const std::string& textureFilePath)
{
	textureFilePath_ = textureFilePath;

	if (!textureFilePath.empty()) {
		// テクスチャ読み込み
		TextureManager::GetInstance()->LoadTexture(textureFilePath);
		textureIndexSRV_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);
	}
}

void YParticleSystem::SetMeshType(int meshType, const nlohmann::json& params)
{
	currentMeshType_ = meshType;

	// パラメータ配列を読み込み
	if (params.is_array()) {
		size_t paramCount = std::min(params.size(), size_t(4));
		for (size_t i = 0; i < paramCount; ++i) {
			meshParams_[i] = params[i].get<float>();
		}
	}

	// メッシュを再生成
	RefreshMesh();
}

void YParticleSystem::SetMeshTypeAndParams(int meshType, const float* params)
{
	currentMeshType_ = meshType;

	if (params) {
		for (int i = 0; i < 4; ++i) {
			meshParams_[i] = params[i];
		}
	}

	// メッシュを再生成
	RefreshMesh();
}

int YParticleSystem::FindEmptyIndex() const
{
	for (uint32_t i = 0; i < maxParticles_; ++i) {
		if (!attributes_[i].isActive) return i;
	}
	return -1;
}

void YParticleSystem::RefreshMesh()
{
	switch (currentMeshType_) {
	case 0: mesh_ = MeshPrimitive::CreatePlane(meshParams_[0], meshParams_[1]); break;
	case 1: mesh_ = MeshPrimitive::CreateBox(meshParams_[0], meshParams_[1], meshParams_[2]); break;
	case 2: mesh_ = MeshPrimitive::CreateRing(meshParams_[0], meshParams_[1], (uint32_t)meshParams_[2]); break;
	case 3: mesh_ = MeshPrimitive::CreateCylinder(meshParams_[0], meshParams_[1], (uint32_t)meshParams_[2], meshParams_[3]); break;
	case 4: mesh_ = MeshPrimitive::CreateFanShape(meshParams_[0], meshParams_[1], (uint32_t)meshParams_[2]); break;
	case 5: mesh_ = MeshPrimitive::CreateSphere(meshParams_[0], (uint32_t)meshParams_[2]); break;
	}
}

std::shared_ptr<IParticleModule> YParticleSystem::GetModule(const std::string& moduleName) const
{
	// Spawnモジュールから検索
	for (const auto& m : spawnModules_) {
		if (m->GetName() == moduleName) return m;
	}
	// Updateモジュールから検索
	for (const auto& m : updateModules_) {
		if (m->GetName() == moduleName) return m;
	}
	return nullptr;
}

#ifdef USE_IMGUI
void YParticleSystem::ShowEditor()
{
	ImGui::Text("システム: %s", name_.c_str());

	// --- テクスチャ設定 ---
	if (ImGui::TreeNode("テクスチャ設定")) {
		ImGui::Text("パス: %s", textureFilePath_.c_str());

		if (ImGui::Button("テクスチャを開く")) {
			textureBrowser_.Scan(); // 最新状態に更新してから開く
			showTextureBrowser_ = true;
		}

		// ポップアップでインラインブラウザを表示
		if (showTextureBrowser_) {
			ImGui::OpenPopup("##TextureBrowser");
		}
		ImGui::SetNextWindowSize(ImVec2(500, 420), ImGuiCond_Appearing);
		if (ImGui::BeginPopupModal("##TextureBrowser", &showTextureBrowser_,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
		{
			ImGui::Text("テクスチャを選択");
			ImGui::Separator();
			// Grid モードで表示（クリックでコールバックが発火し自動クローズ）
			textureBrowser_.Draw("##TexBrowserChild", ImVec2(0, 340));
			ImGui::Separator();
			if (ImGui::Button("キャンセル", ImVec2(-1, 0))) {
				showTextureBrowser_ = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::TreePop();
	}


	// --- メッシュ設定 ---
	if (ImGui::TreeNode("メッシュ設定")) {
		bool changed = false;
		const char* meshTypes[] = { "平面", "ボックス", "リング", "シリンダー","扇形","円形"};

		// Comboでタイプ選択
		if (ImGui::Combo("プリミティブタイプ", &currentMeshType_, meshTypes, IM_ARRAYSIZE(meshTypes))) {
			changed = true;
		}

		// 選択されたタイプに応じて表示するスライダーを切り替え
		switch (currentMeshType_) {
		case 0: // Plane(w, h)
			changed |= ImGui::DragFloat("幅", &meshParams_[0], 0.1f, 0.1f, 100.0f);
			changed |= ImGui::DragFloat("高さ", &meshParams_[1], 0.1f, 0.1f, 100.0f);
			break;
		case 1: // Box(w, h, d)
			changed |= ImGui::DragFloat("幅", &meshParams_[0], 0.1f, 0.1f, 100.0f);
			changed |= ImGui::DragFloat("高さ", &meshParams_[1], 0.1f, 0.1f, 100.0f);
			changed |= ImGui::DragFloat("奥行き", &meshParams_[2], 0.1f, 0.1f, 100.0f);
			break;
		case 2: // Ring(outer, inner, divide)
			changed |= ImGui::DragFloat("外側半径", &meshParams_[0], 0.1f, 0.1f, 100.0f);
			changed |= ImGui::DragFloat("内側半径", &meshParams_[1], 0.1f, 0.0f, 100.0f);
			{
				int div = (int)meshParams_[2];
				if (ImGui::DragInt("分割数", &div, 1, 3, 128)) {
					meshParams_[2] = (float)div;
					changed = true;
				}
			}
			break;
		case 3: // Cylinder(outer, inner, divide, height)
			changed |= ImGui::DragFloat("外側半径", &meshParams_[0], 0.1f, 0.1f, 100.0f);
			changed |= ImGui::DragFloat("内側半径", &meshParams_[1], 0.1f, 0.0f, 100.0f);
			changed |= ImGui::DragFloat("高さ", &meshParams_[3], 0.1f, 0.1f, 100.0f);
			{
				int div = (int)meshParams_[2];
				if (ImGui::DragInt("分割数", &div, 1, 3, 128)) {
					meshParams_[2] = (float)div;
					changed = true;
				}
			}
			break;

		case 4: // Fan Shape (radius, angle, divide)
			changed |= ImGui::DragFloat("半径", &meshParams_[0], 0.1f, 0.1f, 100.0f);
			changed |= ImGui::DragFloat("角度", &meshParams_[1], 1.0f, 1.0f, 360.0f);
			{
				int div = (int)meshParams_[2];
				if (ImGui::DragInt("分割数", &div, 1, 3, 128)) {
					meshParams_[2] = (float)div;
					changed = true;
				}
			}

		case 5: // Sphere (radius, -, divide)
			changed |= ImGui::DragFloat("半径", &meshParams_[0], 0.1f, 0.1f, 100.0f);
			{
				int div = (int)meshParams_[2];
				if (ImGui::DragInt("分割数", &div, 1, 3, 128)) {
					meshParams_[2] = (float)div;
					changed = true;
				}
			}
			break;
		}

		// 値が変わった場合のみ再生成（メモリリーク防止のため）
		if (changed || mesh_ == nullptr) {
			RefreshMesh();
		}
		ImGui::TreePop();
	}

	// --- ビルボード設定 ---
	if (ImGui::TreeNode("ビルボード設定")) {
		const char* billboardTypes[] = { "なし", "完全ビルボード", "Y軸固定" };
		int currentType = static_cast<int>(billboardType_);

		if (ImGui::Combo("ビルボードタイプ", &currentType, billboardTypes, IM_ARRAYSIZE(billboardTypes))) {
			billboardType_ = static_cast<BillboardType>(currentType);
		}

		// 説明文
		ImGui::Separator();
		ImGui::TextWrapped("ビルボードタイプ:");
		ImGui::BulletText("なし: ビルボードなしの通常メッシュレンダリング");
		ImGui::BulletText("完全ビルボード: 常にカメラに向く");
		ImGui::BulletText("Y軸固定: Y軸のみ回転（地面エフェクト用）");

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("ライティング設定")) {
		ImGui::Separator();

		// --- 各ライトの有効/無効 ---
		ImGui::Text("ライトの有効化");
		ImGui::Checkbox("ディレクショナルライト", &lightSetting_.enableDirectionalLight);

		ImGui::Spacing();

		// --- 強度スライダー（有効なライトのみ表示）---
		if (lightSetting_.enableDirectionalLight) {
			ImGui::SliderFloat("ディレクショナル強度", &lightSetting_.directionalIntensity, 0.0f, 2.0f);
		}

		ImGui::TreePop();
	}

	// ブレンドモード
	const char* blendNames[] = { "なし", "通常", "加算", "減算", "乗算", "スクリーン" };
	int currentMode = (int)blendMode_;
	if (ImGui::Combo("ブレンドモード", &currentMode, blendNames, IM_ARRAYSIZE(blendNames))) {
		SetBlendMode((BlendMode)currentMode);
	}

	// === Spawn Modules ===
	if (ImGui::TreeNode("スポーンモジュール")) {
		ImGui::Spacing();

		// 既存モジュールの表示と編集
		if (!spawnModules_.empty()) {
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "登録済みモジュール (%d個)", (int)spawnModules_.size());
			ImGui::Separator();

			for (int i = 0; i < spawnModules_.size(); ++i) {
				ImGui::PushID(i);

				// モジュール名をヘッダーとして表示
				bool nodeOpen = ImGui::TreeNodeEx(spawnModules_[i]->GetName().c_str(),
					ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed);

				if (nodeOpen) {
					ImGui::Indent();

					// 各モジュールのエディタを表示
					spawnModules_[i]->DrawEditor();

					ImGui::Spacing();

					// 削除ボタン（赤色で目立たせる）
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.1f, 0.1f, 1.0f));

					if (ImGui::Button("削除", ImVec2(60, 0))) {
						DeleteSpawnModule(spawnModules_[i]);
						ImGui::PopStyleColor(3);
						ImGui::Unindent();
						ImGui::TreePop();
						ImGui::PopID();
						break; // イテレータ破壊を避けるため
					}

					ImGui::PopStyleColor(3);
					ImGui::Unindent();
					ImGui::TreePop();
				}

				ImGui::PopID();
				ImGui::Spacing();
			}
		}
		else {
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "登録されているモジュールはありません");
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 新規追加（コンボボックス形式）
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "モジュールを追加");

		struct SpawnModuleEntry {
			const char* name;
			std::function<std::shared_ptr<ISpawnModule>()> factory;
		};
		static SpawnModuleEntry availableSpawnModules[] = {
			{ "速度", []() { return std::make_shared<SpawnVelocity>(); } },
			{  "UV", []() { return std::make_shared<SpawnUV>(); } },
			{ "コーン速度", []() { return std::make_shared<SpawnConeVelocity>(); } },
			{ "放射状速度", []() { return std::make_shared<SpawnRadialVelocity>(); } },
			{ "ランダムカラー", []() { return std::make_shared<SpawnRandomColor>(); } },
			{ "ランダムスケール", []() { return std::make_shared<SpawnRandomScale>(); } },
			{ "ランダム回転", []() { return std::make_shared<SpawnRandomRotate>(); } },
			{ "ライフタイム", []() { return std::make_shared<SpawnLifeTime>(); } },
		};

		static int selectedSpawnModule = 0;

		// コンボボックスでモジュールを選択
		ImGui::SetNextItemWidth(-80);
		ImGui::Combo("##SpawnModuleSelect", &selectedSpawnModule,
			[](void* data, int idx, const char** out_text) {
				*out_text = ((SpawnModuleEntry*)data)[idx].name;
				return true;
			},
			availableSpawnModules, IM_ARRAYSIZE(availableSpawnModules));

		ImGui::SameLine();

		// 追加ボタン（緑色で目立たせる）
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));

		if (ImGui::Button("追加", ImVec2(70, 0))) {
			AddSpawnModule(availableSpawnModules[selectedSpawnModule].factory());
		}

		ImGui::PopStyleColor(3);

		ImGui::TreePop();
	}

	// === Update Modules ===
	if (ImGui::TreeNode("更新モジュール")) {
		ImGui::Spacing();

		// 既存モジュールの表示と編集
		if (!updateModules_.empty()) {
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "登録済みモジュール (%d個)", (int)updateModules_.size());
			ImGui::Separator();

			for (int i = 0; i < updateModules_.size(); ++i) {
				ImGui::PushID(i);

				// モジュール名をヘッダーとして表示
				bool nodeOpen = ImGui::TreeNodeEx(updateModules_[i]->GetName().c_str(),
					ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed);

				if (nodeOpen) {
					ImGui::Indent();

					// 各モジュールのエディタを表示
					updateModules_[i]->DrawEditor();

					ImGui::Spacing();

					// 削除ボタン（赤色で目立たせる）
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.1f, 0.1f, 1.0f));

					if (ImGui::Button("削除", ImVec2(60, 0))) {
						DeleteUpdateModule(updateModules_[i]);
						ImGui::PopStyleColor(3);
						ImGui::Unindent();
						ImGui::TreePop();
						ImGui::PopID();
						break;
					}

					ImGui::PopStyleColor(3);
					ImGui::Unindent();
					ImGui::TreePop();
				}

				ImGui::PopID();
				ImGui::Spacing();
			}
		}
		else {
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "登録されているモジュールはありません");
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 新規追加（コンボボックス形式）
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "モジュールを追加");

		struct UpdateModuleEntry {
			const char* name;
			std::function<std::shared_ptr<IUpdateModule>()> factory;
		};
		static UpdateModuleEntry availableUpdateModules[] = {
			{ "速度", []() { return std::make_shared<UpdateVelocity>(); } },
			{  "UVスクロール", []() { return std::make_shared<UpdateUVScroll>(); } },
			{ "重力", []() { return std::make_shared<UpdateGravity>(); } },
			{ "カラー", []() { return std::make_shared<UpdateColor>(); } },
			{ "アトラクタ", []() { return std::make_shared<UpdateAttractor>(); } },
			{ "衝突", []() { return std::make_shared<UpdateCollision>(); } },
			{ "グラデーション", []() { return std::make_shared<UpdateColorGradient>(); } },
			{ "ドラッグ", []() { return std::make_shared<UpdateDrag>(); } },
			{ "ノイズ", []() { return std::make_shared<UpdateNoise>(); } },
			{ "軌道", []() { return std::make_shared<UpdateOrbital>(); } },
			{ "パルス", []() { return std::make_shared<UpdatePulse>(); } },
			{ "回転", []() { return std::make_shared<UpdateRotation>(); } },
			{ "スケール", []() { return std::make_shared<UpdateScale>(); } },
			{ "スパイラル収束", []() { return std::make_shared<UpdateSpiralConverge>(); } },
			{ "フェードアウト", []() { return std::make_shared<UpdateFadeOut>(); } },
		};

		static int selectedUpdateModule = 0;

		// コンボボックスでモジュールを選択
		ImGui::SetNextItemWidth(-80);
		ImGui::Combo("##UpdateModuleSelect", &selectedUpdateModule,
			[](void* data, int idx, const char** out_text) {
				*out_text = ((UpdateModuleEntry*)data)[idx].name;
				return true;
			},
			availableUpdateModules, IM_ARRAYSIZE(availableUpdateModules));

		ImGui::SameLine();

		// 追加ボタン（緑色で目立たせる）
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));

		if (ImGui::Button("追加", ImVec2(70, 0))) {
			AddUpdateModule(availableUpdateModules[selectedUpdateModule].factory());
		}

		ImGui::PopStyleColor(3);

		ImGui::TreePop();
	}
}
#endif
