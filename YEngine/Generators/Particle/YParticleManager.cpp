#include "YParticleManager.h"
#include <chrono>
#include "Graphics/LightManager/LightManager.h"
#include <PipelineManager/YPipelineManager.h>
#include "YEmitterGroupManager.h"
#include "DirectXCommon.h"

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
// 描画
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
			if (batch.mesh == mesh && batch.textureIndex == texIndex
				&& batch.lightSetting == system->GetLightSetting()) {
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
			newBatch.lightSetting = system->GetLightSetting();   // ← 追加
			newBatch.systems.push_back(system.get());
			batches.push_back(newBatch);
		}
	}
}

void YParticleManager::Draw() {
	if (!initialized_ || !renderer_ || !camera_) return;

	auto startTime = std::chrono::high_resolution_clock::now();

	// レンダーバッチを作成
	std::vector<RenderBatch> batches;
	CreateRenderBatches(batches);

	// パイプラインセット
	auto commandList = YoRigine::DirectXCommon::GetInstance()->GetCommandList();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());

	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("YParticle");
	// ライトマネージャーにコマンドリストをセット
	YoRigine::LightManager::GetInstance()->SetCommandList(indices.at("gDirectionalLight"), indices.at("gPointLight"), indices.at("gSpotLight"));

	// バッチごとに描画
	renderer_->BeginFrame();
	for (const auto& batch : batches) {

		// バッチのライト設定を Renderer に反映
		renderer_->ApplyLightSetting(batch.lightSetting);

		// バッチ内のすべてのシステムを追加
		for (auto* system : batch.systems) {
			renderer_->AddSystem(*system, camera_);
		}

		// まとめて描画
		renderer_->EndFrame(batch.mesh, batch.textureIndex);
	}

	auto endTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
	stats_.renderTimeMs = duration.count() / 1000.0f;
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