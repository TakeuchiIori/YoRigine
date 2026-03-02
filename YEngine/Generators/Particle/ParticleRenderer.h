#pragma once
// C++
#include <wrl.h>

// Engine
#include "DirectXCommon.h"
#include "Systems/Camera/Camera.h"
#include "ParticleSystem.h"
#include "LightManager/LightManager.h"

/// <summary>
/// CPUパーティクルの描画クラス(ライティング対応版)
/// </summary>
class ParticleRenderer
{
public:
	///************************* 基本関数 *************************///
	ParticleRenderer() = default;
	~ParticleRenderer() = default;
	void Initialize(SrvManager* srvManager);
	void Finalize();
	void RenderSystem(ParticleSystem& system);
	void RenderTrails(ParticleSystem& system);
private:
	///************************* 内部処理 *************************///
	void CreateMaterialResource();
	void SetPipeline(BlendMode blendMode);
	void UpdateMaterialData(const ParticleSystem& system);
	void UpdateInstanceData(ParticleSystem& system);
	void SetupTexture(const std::string& textureFilePath, uint32_t textureIndexSRV);
	void DrawInstances(const std::shared_ptr<Mesh>& mesh, uint32_t instanceCount, uint32_t srvIndex);

public:
	///************************* アクセッサ *************************///
	Camera* GetCamera() const { return camera_; }
	void SetCamera(Camera* camera) { camera_ = camera; }

	// LightManager設定
	void SetLightManager(YoRigine::LightManager* lightManager) { lightManager_ = lightManager; }
	YoRigine::LightManager* GetLightManager() const { return lightManager_; }

private:
	///************************* GPU用の構造体 *************************///
	// マテリアルリソース
	struct Material {
		Matrix4x4 uvTransform;
		Vector4 color;
		int32_t enableLighting;  // ライティング有効フラグ
		float padding[3];
	};

	///************************* メンバ変数 *************************///
	YoRigine::DirectXCommon* dxCommon_;
	SrvManager* srvManager_;
	Camera* camera_;
	YoRigine::LightManager* lightManager_ = nullptr;  // LightManagerへの参照

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_;

	// パイプライン
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> trailRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> trailPipelineState_;
};