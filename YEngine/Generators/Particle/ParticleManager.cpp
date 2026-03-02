#include "ParticleManager.h"

// Engine
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "loaders./Texture./TextureManager.h"
#include "WinApp./WinApp.h"
#include "Debugger/Logger.h"
#include "ParticleEditor.h"

// C++
#include <numbers>
#include <cassert>
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace YoRigine {
	/// <summary>
	/// インスタンスの本体
	/// </summary>
	std::unique_ptr<ParticleManager> ParticleManager::instance_ = nullptr;

	/// <summary>
	/// call_once 用フラグ
	/// </summary>
	std::once_flag ParticleManager::initFlag_;

	/// <summary>
	/// ParticleManager のシングルトン取得
	/// </summary>
	ParticleManager* ParticleManager::GetInstance() {
		std::call_once(initFlag_, []() {
			instance_ = std::make_unique<ParticleManager>();
			});
		return instance_.get();
	}


	/// <summary>
	/// ParticleManager 全体の解放
	/// </summary>
	void ParticleManager::Finalize() {
		// レンダラーの解放
		if (renderer_) {
			renderer_->Finalize();
			renderer_.reset();
		}

		// 各パーティクルシステムの解放
		for (auto& [name, system] : systems_) {
			system->Finalize();
			system->FinalizeTrailResources();
		}
		systems_.clear();

		// シングルトン破棄
		instance_.reset();
	}

	//=================================================================
	// 初期化
	//=================================================================

	/// <summary>
	/// ParticleManager 初期化
	/// </summary>
	/// <param name="srvManager">SRV ハンドラ</param>
	void ParticleManager::Initialize(SrvManager* srvManager) {
		if (initialized_) return;

		srvManager_ = srvManager;

		// レンダラー生成＆初期化
		renderer_ = std::make_unique<ParticleRenderer>();
		renderer_->Initialize(srvManager);

		initialized_ = true;
	}

	/// <summary>
	/// パーティクルグループ（パーティクルシステム）の生成
	/// </summary>
	/// <param name="name">システム名</param>
	/// <param name="textureFilePath">使用するテクスチャ</param>
	void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath) {
		// すでに作成済みなら何もしない
		if (systems_.find(name) != systems_.end()) return;

		// ParticleEditor にも登録（エディター UI で編集可能にする）
		ParticleEditor::GetInstance().RegisterSystem(name);

		// パーティクルシステム作成
		auto system = std::make_unique<ParticleSystem>(name);

		// 使用テクスチャをセット
		system->SetTexture(textureFilePath);

		// DirectX12 用リソース初期化
		if (srvManager_) {
			system->InitializeResources(srvManager_);

			// トレイル機能が有効ならトレイル専用リソースも作成
			if (system->GetSettings().GetTrailEnabled()) {
				system->InitializeTrailResources(srvManager_);
			}
		}

		// 管理リストへ登録
		systems_[name] = std::move(system);
	}
	//=================================================================
	// プリミティブ（Mesh）設定
	//=================================================================

	/// <summary>
	/// パーティクル描画に使用する Mesh をセットする
	/// </summary>
	/// <param name="groupName">対象パーティクルシステム名</param>
	/// <param name="mesh">メッシュ共有ポインタ</param>
	void ParticleManager::SetPrimitiveMesh(const std::string& groupName, const std::shared_ptr<Mesh>& mesh) {
		auto it = systems_.find(groupName);
		if (it != systems_.end()) {
			it->second->SetMesh(mesh);
		}
	}

	//=================================================================
	// パーティクル発生
	//=================================================================

	/// <summary>
	/// 通常の Emit（一定数を現在位置で生成）
	/// </summary>
	/// <param name="name">システム名</param>
	/// <param name="position">生成位置</param>
	/// <param name="count">生成数</param>
	void ParticleManager::Emit(const std::string& name, const Vector3& position, uint32_t count) {
		auto it = systems_.find(name);
		if (it != systems_.end()) {
			it->second->Emit(position, count);
		}
	}

	/// <summary>
	/// バースト（瞬間的に大量発生）
	/// </summary>
	/// <param name="groupName">システム名</param>
	/// <param name="position">生成位置</param>
	/// <param name="count">生成数</param>
	void ParticleManager::EmitBurst(const std::string& groupName, const Vector3& position, int count) {
		auto it = systems_.find(groupName);
		if (it != systems_.end()) {
			it->second->EmitBurst(position, count);
		}
	}

	//=================================================================
	// システム取得
	//=================================================================

	/// <summary>
	/// 対象名のパーティクルシステムを取得
	/// </summary>
	ParticleSystem* ParticleManager::GetSystem(const std::string& name) {
		auto it = systems_.find(name);
		return it != systems_.end() ? it->second.get() : nullptr;
	}

	//=================================================================
	// パラメータ変更系
	//=================================================================

	/// <summary>
	/// 重力を上書きする
	/// </summary>
	void ParticleManager::SetGravity(const std::string& name, const Vector3& gravity) {
		auto* system = GetSystem(name);
		if (system) {
			system->GetSettings().SetGravity(gravity);
		}
	}

	/// <summary>
	/// 色を設定（開始色と終了色）
	/// </summary>
	void ParticleManager::SetColor(const std::string& name, const Vector4& startColor, const Vector4& endColor) {
		auto* system = GetSystem(name);
		if (system) {
			system->GetSettings().SetStartColor(startColor);
			system->GetSettings().SetEndColor(endColor);
		}
	}

	/// <summary>
	/// パーティクル発生レート（1秒あたりの生成数）
	/// </summary>
	void ParticleManager::SetEmissionRate(const std::string& name, float rate) {
		auto* system = GetSystem(name);
		if (system) {
			system->GetSettings().SetEmissionRate(rate);
		}
	}

	/// <summary>
	/// パーティクル速度設定
	/// </summary>
	void ParticleManager::SetSpeed(const std::string& name, float speed) {
		auto* system = GetSystem(name);
		if (system) {
			system->GetSettings().SetSpeed(speed);
		}
	}

	/// <summary>
	/// パーティクル寿命の範囲を設定
	/// </summary>
	void ParticleManager::SetLifeTime(const std::string& name, const Vector2& lifeTimeRange) {
		auto* system = GetSystem(name);
		if (system) {
			system->GetSettings().SetLifeTimeRange(lifeTimeRange);
		}
	}

	/// <summary>
	/// ブレンドモードの設定（Add・Alpha など）
	/// </summary>
	void ParticleManager::SetBlendMode(const std::string& name, BlendMode blendMode) {
		auto* system = GetSystem(name);
		if (system) {
			system->GetSettings().SetBlendMode(blendMode);
		}
	}

	//=================================================================
	// 更新処理（パフォーマンス測定付き）
	//=================================================================

	/// <summary>
	/// パーティクルシステム全体の更新
	/// </summary>
	/// <param name="deltaTime">経過時間</param>
	void ParticleManager::Update(float deltaTime) {
		auto updateStart = std::chrono::high_resolution_clock::now();

		performanceInfo_.totalParticles = 0;
		performanceInfo_.activeGroups = 0;

		// すべてのシステムを更新
		for (auto& [name, system] : systems_) {
			if (system->IsActive()) {

				// システム内部の更新処理（Emit・物理計算など）
				system->Update(deltaTime);

				// パフォーマンス統計
				performanceInfo_.totalParticles += static_cast<int>(system->GetParticleCount());
				performanceInfo_.activeGroups++;
			}
		}

		// 経過時間をミリ秒で保存
		auto updateEnd = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(updateEnd - updateStart);
		performanceInfo_.updateTime = duration.count() / 1000.0f;
	}

	//=================================================================
	// 描画処理
	//=================================================================

	/// <summary>
	/// 全パーティクルの描画
	/// </summary>
	void ParticleManager::Draw() {
		if (!renderer_) return;

		auto renderStart = std::chrono::high_resolution_clock::now();

		// すべてのパーティクルシステムを描画
		for (auto& [name, system] : systems_) {
			if (system->IsActive() && system->GetParticleCount() > 0) {

				// 通常パーティクル描画
				renderer_->RenderSystem(*system);

				// トレイルが有効ならトレイルも描画
				if (system->GetSettings().GetTrailEnabled()) {

					// カメラ行列に基づくトレイル生成
					system->PrepareTrailData(renderer_->GetCamera());

					// トレイル描画
					renderer_->RenderTrails(*system);
				}
			}
		}

		// 経過時間をミリ秒で保存
		auto renderEnd = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(renderEnd - renderStart);
		performanceInfo_.renderTime = duration.count() / 1000.0f;
	}

	/// <summary>
	/// パーティクルレンダラーに使用するカメラを設定
	/// </summary>
	void ParticleManager::SetCamera(Camera* camera) {
		if (renderer_) {
			renderer_->SetCamera(camera);
		}
	}

	/// <summary>
	/// 登録されているパーティクルシステム名一覧
	/// </summary>
	std::vector<std::string> ParticleManager::GetAllSystemNames() const {
		std::vector<std::string> names;
		names.reserve(systems_.size());

		for (const auto& [name, system] : systems_) {
			names.push_back(name);
		}
		return names;
	}
}