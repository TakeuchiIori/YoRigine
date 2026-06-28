#include "YParticleManager.h"
#include <chrono>
#include "Graphics/LightManager/LightManager.h"
#include <PipelineManager/YPipelineManager.h>
#include "YEmitterGroupManager.h"
#include "DirectXCommon.h"
#include "YParticleModuleFactory.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

//=================================================================
// 初期化・終了
//=================================================================

void YParticleManager::Initialize(SrvManager* srvManager, uint32_t maxTotalParticles) {
	if (initialized_) return;

	srvManager_ = srvManager;

	// レンダラーの初期化
	renderer_ = std::make_unique<YParticleRenderer>();
	renderer_->Initialize(srvManager, maxTotalParticles);

	// パイプライン取得
	auto pm = YPipelineManager::GetInstance();
	rootSignature_ = pm->GetRootSignature("YParticle");
	pipelineState_ = pm->GetPipeLineStateObject("YParticle");

	initialized_ = true;
}

void YParticleManager::Finalize() {
	if (!initialized_) return;

	// すべてのシステムを削除
	systems_.clear();

	// レンダラーの解放
	renderer_.reset();

	initialized_ = false;
}

//=================================================================
// システム管理
//=================================================================

YParticleSystem* YParticleManager::CreateSystem(const std::string& name, uint32_t maxParticles) {
	// すでに存在する場合は既存のものを返す
	if (systems_.find(name) != systems_.end()) {
		return systems_[name].get();
	}

	// 新しいシステムを作成
	auto system = std::make_unique<YParticleSystem>(name, maxParticles);
	YParticleSystem* ptr = system.get();
	systems_[name] = std::move(system);

	return ptr;
}

YParticleSystem* YParticleManager::GetSystem(const std::string& name) {
	auto it = systems_.find(name);
	if (it != systems_.end()) {
		return it->second.get();
	}
	return nullptr;
}

void YParticleManager::RemoveSystem(const std::string& name) {
	systems_.erase(name);
}

bool YParticleManager::RenameSystem(const std::string& oldName, const std::string& newName) {
	// 新しい名前が既に存在する場合は失敗
	if (systems_.count(newName) > 0) {
		return false;
	}

	auto it = systems_.find(oldName);
	if (it == systems_.end()) {
		return false;
	}

	// システムを取り出して名前を変更
	auto system = std::move(it->second);
	systems_.erase(it);

	// 新しい名前で再登録（内部の name_ は YParticleSystem 側で変更する必要あり）
	systems_[newName] = std::move(system);

	return true;
}

std::vector<std::string> YParticleManager::GetAllSystemNames() const {
	std::vector<std::string> names;
	names.reserve(systems_.size());

	for (const auto& [name, system] : systems_) {
		names.push_back(name);
	}

	return names;
}

#include <fstream>
#include <set>

bool YParticleManager::LoadSystemsFromFile(const std::string& filePath) {
	try {
		std::ifstream file(filePath);
		if (!file.is_open()) return false;

		nlohmann::json json;
		file >> json;
		LoadSystemsFromJson(json);
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

void YParticleManager::LoadSystemsFromJson(const nlohmann::json& json) {
	if (!json.contains("name")) return;

	std::string name = json["name"];
	uint32_t maxP = json.value("maxParticles", 1000);

	auto& manager = YParticleManager::GetInstance();
	auto* system = manager.CreateSystem(name, maxP);
	if (!system) return;

	if (json.contains("texture"))      system->SetTexture(json["texture"]);
	if (json.contains("isRelative"))   system->SetRelative(json["isRelative"]);
	if (json.contains("billboardType"))
		system->SetBillboardType(static_cast<BillboardType>(json["billboardType"].get<uint32_t>()));
	if (json.contains("BlendMode"))
		system->SetBlendMode(static_cast<BlendMode>(json["BlendMode"].get<int>()));
	if (json.contains("Lighting"))
		system->SetLightSetting(json["Lighting"].get<bool>() ? ParticleLightSetting{ true, true, true } : ParticleLightSetting{ false, false, false });
	if (json.contains("mesh")) {
		const auto& meshJson = json["mesh"];
		if (meshJson.contains("type") && meshJson.contains("params"))
			system->SetMeshType(meshJson["type"], meshJson["params"]);
	}

	if (json.contains("spawnModules")) {
		for (const auto& mj : json["spawnModules"]) {
			if (!mj.contains("type")) continue;
			auto module = YParticleModuleFactory::GetInstance().CreateSpawnModule(mj["type"]);
			if (module) {
				if (mj.contains("data")) module->LoadFromJson(mj["data"]);
				system->AddSpawnModule(module);
			}
		}
	}

	if (json.contains("updateModules")) {
		for (const auto& mj : json["updateModules"]) {
			if (!mj.contains("type")) continue;
			auto module = YParticleModuleFactory::GetInstance().CreateUpdateModule(mj["type"]);
			if (module) {
				if (mj.contains("data")) module->LoadFromJson(mj["data"]);
				system->AddUpdateModule(module);
			}
		}
	}
}

bool YParticleManager::SaveSystemsToFile(const std::string& filePath) const {
	try {
		nlohmann::json json;
		json["version"] = "1.0";
		json["systems"] = nlohmann::json::array();

		for (const auto& [name, system] : systems_) {
			nlohmann::json sysJson;
			sysJson["name"] = system->GetName();
			sysJson["maxParticles"] = system->GetMaxParticles();
			sysJson["texture"] = system->GetTextureFilePath();
			// ... その他の設定
			json["systems"].push_back(sysJson);
		}

		std::ofstream file(filePath);
		if (!file.is_open()) return false;
		file << json.dump(4);
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

//=================================================================
// 更新
//=================================================================

void YParticleManager::Update(float deltaTime) {
	if (!initialized_) return;

	auto startTime = std::chrono::high_resolution_clock::now();

	// 統計情報のリセット
	stats_.totalSystems = systems_.size();
	stats_.activeSystems = 0;
	stats_.totalActiveParticles = 0;

	// すべてのシステムを更新
	for (auto& [name, system] : systems_) {
		system->Update(deltaTime);
		// アクティブなパーティクル数をカウント
		size_t activeCount = 0;
		const auto& attributes = system->GetAttributes();
		for (const auto& attr : attributes) {
			if (attr.isActive) {
				activeCount++;
			}
		}

		if (activeCount > 0) {
			stats_.activeSystems++;
			stats_.totalActiveParticles += activeCount;
		}
	}

	auto endTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
	stats_.updateTimeMs = duration.count() / 1000.0f;


	YEmitterGroupManager::GetInstance().Update(deltaTime);
}

//=================================================================
// 描画　（「描画設定が同じシステムをグループ化して、後でまとめて描画するための仕分け処理）
//=================================================================
void YParticleManager::CreateRenderBatches(std::vector<RenderBatch>& batches) {
	batches.clear();

	for (auto& [name, system] : systems_) {
		auto mesh = system->GetMesh();
		uint32_t texIndex = system->GetTextureIndex();

		// メッシュが未設定の場合はスキップ
		if (!mesh) continue;

		// アクティブなパーティクルがあるかチェック
		bool hasActiveParticles = false;
		const auto& attributes = system->GetAttributes();
		for (const auto& attr : attributes) {
			if (attr.isActive) {
				hasActiveParticles = true;
				break;
			}
		}

		if (!hasActiveParticles) continue;

		// 既存のバッチを検索（同じメッシュ＋テクスチャ＋ライト設定）
		bool foundBatch = false;
		for (auto& batch : batches) {
			if (batch.mesh == mesh 
				&& batch.textureIndex == texIndex
				&& batch.lightSetting == system->GetLightSetting()
				&& batch.blendMode == system->GetBlendMode()) {
				batch.systems.push_back(system.get());
				foundBatch = true;
				break;
			}
		}

		// 新しいバッチを作成
		if (!foundBatch) {
			RenderBatch newBatch;
			newBatch.mesh = mesh;
			newBatch.textureIndex = texIndex;
			newBatch.lightSetting = system->GetLightSetting();
			newBatch.blendMode = system->GetBlendMode();
			newBatch.systems.push_back(system.get());
			batches.push_back(newBatch);
		}
	}
}

void YParticleManager::Draw() {
	if (!initialized_ || !renderer_ || !camera_) return;

	std::vector<RenderBatch> batches;
	CreateRenderBatches(batches);

	auto commandList = YoRigine::DirectXCommon::GetInstance()->GetCommandList();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());


	renderer_->BeginFrame();
	for (const auto& batch : batches) {
		renderer_->ApplyLightSetting(batch.lightSetting);

		// バッチ内の全システムをまずバッファに書く
		for (auto* system : batch.systems) {
			renderer_->AddSystem(*system, camera_);
		}

		// 書き終わったら1回だけDrawする（バッチ統合の本来の姿）
		renderer_->EndFrame(batch.mesh, batch.textureIndex, batch.blendMode);
	}
}
//=================================================================
// 便利メソッド
//=================================================================

void YParticleManager::Emit(const std::string& systemName, const Vector3& position, int count) {
	auto* system = GetSystem(systemName);
	if (system) {
		system->Emit(position, count);
	}
}

void YParticleManager::EmitBurst(const std::string& systemName, const Vector3& position, int count) {
	auto* system = GetSystem(systemName);
	if (system) {
		system->Emit(position, count);
	}
}

//=================================================================
// バンドルロード・セーブ
//=================================================================

bool YParticleManager::LoadEffectBundle(const std::string& filePath) {
	try {
		std::ifstream file(filePath);
		if (!file.is_open()) return false;
		nlohmann::json j;
		file >> j;

		if (j.contains("systems")) {
			for (const auto& sysJson : j["systems"]) {
				LoadSystemsFromJson(sysJson);
			}
		}
		if (j.contains("groups")) {
			YEmitterGroupManager::GetInstance().LoadAllFromJson(j);
		}
		return true;
	}
	catch (...) { return false; }
}

bool YParticleManager::SaveEffectBundle(const std::string& groupName, const std::string& filePath) {
	auto& groupMgr = YEmitterGroupManager::GetInstance();
	auto* group = groupMgr.GetGroup(groupName);
	if (!group) return false;

	try {
		nlohmann::json j;
		j["systems"] = nlohmann::json::array();
		j["groups"] = nlohmann::json::array();

		// グループが参照するシステム名を収集
		std::set<std::string> systemNames;
		for (size_t i = 0; i < group->GetEmitterCount(); ++i) {
			auto* emitter = group->GetEmitter(i);
			if (emitter) systemNames.insert(emitter->GetSystemName());
		}

		// 参照システムをシリアライズ
		for (const auto& sysName : systemNames) {
			auto* system = GetSystem(sysName);
			if (!system) continue;

			nlohmann::json sysJson;
			sysJson["name"] = system->GetName();
			sysJson["maxParticles"] = system->GetMaxParticles();
			sysJson["texture"] = system->GetTextureFilePath();
			sysJson["isRelative"] = system->IsRelative();
			sysJson["billboardType"] = system->GetBillboardTypeAsUInt();
			sysJson["BlendMode"] = static_cast<int>(system->GetBlendMode());
			sysJson["Lighting"] = system->IsEnableLight();
			sysJson["mesh"]["type"] = system->GetCurrentMeshType();
			sysJson["mesh"]["params"] = nlohmann::json::array();
			const float* params = system->GetMeshParams();
			for (int pi = 0; pi < 4; ++pi) sysJson["mesh"]["params"].push_back(params[pi]);

			sysJson["spawnModules"] = nlohmann::json::array();
			for (const auto& mod : system->GetSpawnModules()) {
				nlohmann::json mj;
				mj["type"] = mod->GetTypeName();
				mj["name"] = mod->GetName();
				mod->SaveToJson(mj["data"]);
				sysJson["spawnModules"].push_back(mj);
			}

			sysJson["updateModules"] = nlohmann::json::array();
			for (const auto& mod : system->GetUpdateModules()) {
				nlohmann::json mj;
				mj["type"] = mod->GetTypeName();
				mj["name"] = mod->GetName();
				mod->SaveToJson(mj["data"]);
				sysJson["updateModules"].push_back(mj);
			}

			j["systems"].push_back(sysJson);
		}

		j["groups"].push_back(group->SaveToJson());

		std::ofstream file(filePath);
		if (!file.is_open()) return false;
		file << j.dump(4);
		return true;
	}
	catch (...) { return false; }
}