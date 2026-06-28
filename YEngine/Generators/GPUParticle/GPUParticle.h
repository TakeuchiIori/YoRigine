#pragma once

// Engine
#include <DirectXCommon.h>
#include <PipelineManager/YPipelineManager.h>
#include <ComputeShaderManager/ComputeShaderManager.h>
#include <Systems/Camera/Camera.h>
#include <Mesh/Mesh.h>
#include <Mesh/MeshPrimitive.h>

#include "Material/MaterialColor.h"
#include "Material/MaterialUV.h"

// Math
#include <Vector2.h>
#include <Vector3.h>
#include <Vector4.h>
#include <Matrix4x4.h>

// C++
#include <wrl.h>

/// <summary>
/// GPUパーティクルクラス
/// </summary>
class GPUParticle
{
public:
	// 定数
	static const uint32_t kMaxParticles = 500000;		  // 最大パーティクル数
	static const uint32_t kParticlesPerThread = 128;		  // 1スレッド辺りの処理数
	static const uint32_t kThreadsPerGroup = 1024;		  // 1グループ当たりのスレッド数

	// 必要なスレッドグループ数を取得
	static constexpr uint32_t GetRequiredThreadGroups() {
		constexpr uint32_t particlesPerGroup = kThreadsPerGroup * kParticlesPerThread;
		return (kMaxParticles + particlesPerGroup - 1) / particlesPerGroup;
	}

	// 統計情報構造体
	struct ParticleStats {
		uint32_t maxParticles = kMaxParticles;
		int32_t freeListIndex = 0;
		uint32_t freeCount = 0;
		uint32_t activeCount = 0;
		float usagePercent = 0.0f;
		bool isValid = false;
	};
	// 統計情報取得（非同期・軽量版）
	ParticleStats GetCachedStats() const { return cachedStats_; }  // キャッシュ取得

	///************************* GPUバッファ用の構造体 *************************///
	struct ParticleCSForGPU {
		Vector3 translate;
		float	pad0;

		Vector3 lastTranslate;
		float	pad1;

		Vector3 scale;
		float	pad2;

		Vector3 startScale;
		float	pad3;

		Vector3 endScale;
		float	pad4;

		float rotation;
		float lifeTime;
		float currentTime;
		float	pad5;

		Vector3 velocity;
		float	pad6;

		Vector4 color;
		Vector4 startColor;
		Vector4 endColor;

		uint32_t isParent;
		uint32_t isBillBoard;
		uint32_t isActive;
		float	pad7;
	};

	struct PerViewForGPU {
		Matrix4x4 viewProjection;
		Matrix4x4 billboardMatrix;
	};


public:
	///************************* 基本関数 *************************///
	GPUParticle() = default;
	~GPUParticle() = default;

	void Initialize(const std::string& filepath, Camera* camera);
	void Update(ID3D12Resource* resource, ID3D12Resource* paramsResource);
	void Draw();
	void Reset();

#ifdef USE_IMGUI
	void DrawStatsImGui();
#endif

public:
	///************************* アクセッサ *************************///
	uint32_t GetMaxParticles() const { return kMaxParticles; }
	uint32_t GetParticlesPerThread() const { return kParticlesPerThread; }
	uint32_t GetRequiredThreads() const { return (kMaxParticles + kParticlesPerThread - 1) / kParticlesPerThread; }

	D3D12_GPU_DESCRIPTOR_HANDLE GetParticleUavHandleGPU() const { return particleUavHandleGPU_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetParticleSrvHandleGPU() const { return particleSrvHandleGPU_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetFreeListIndexUavHandleGPU() const { return freeListIndexUavHandleGPU_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetFreeListUavHandleGPU() const { return freeListUavHandleGPU_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetActiveCountUavHandleGPU() const { return activeCountUavHandleGPU_; }

	ID3D12Resource* GetParticleResource() const { return particleResource_.Get(); }
	ID3D12Resource* GetFreeListIndexResource() const { return freeListIndexResource_.Get(); }
	ID3D12Resource* GetFreeListResource() const { return freeListResource_.Get(); }
	ID3D12Resource* GetActiveCountResource() const { return activeCountResource_.Get(); }
	ID3D12Resource* GetPerViewResource() const { return perViewResource_.Get(); }
	ID3D12Resource* GetVertexResource() const { return mesh_->GetMeshResource().vertexResource.Get(); }

	PerViewForGPU* GetPerViewData() const { return perViewData_; }

	void SetMesh(std::shared_ptr<Mesh> mesh) { mesh_ = mesh; }
	void SetCamera(Camera* camera) { camera_ = camera; }



	//--------------------------------- マテリアル関連 ---------------------------------//
	Vector4& GetColor() { return materialColor_->GetColor(); }
	void SetMaterialColor(const Vector4& color) { materialColor_->SetColor(color); }
	void SetAlpha(float alpha) { materialColor_->SetAlpha(alpha); }
	void SetUvTransform(const Matrix4x4& uvTransform) { materialUV_->SetUVTransform(uvTransform); }
private:
	///************************* 内部処理 *************************///
	void CreateGPUParticleResource();
	void CreatePerViewResource();
	void CreateVertexResource();

	void CreateUAV();
	void CreateTexture();

	void UpdateMaterial();
	void UpdateLight();
	void UpdatePerView();

	void DispatchInit();
	void DispatchUpdate(ID3D12Resource* resource, ID3D12Resource* paramsResource);

private:
	///************************* メンバ変数 *************************///
	YoRigine::DirectXCommon* dxCommon_;
	YPipelineManager* pipelineManager_;
	ComputeShaderManager* computeShaderManager_;
	Camera* camera_;

	// マテリアル関連
	std::unique_ptr<MaterialColor> materialColor_;
	std::unique_ptr<MaterialUV> materialUV_;

	// データポインタ
	PerViewForGPU* perViewData_;

	// リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> activeCountResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> activeCountReadback_;

	// SRV/UAVハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE particleSrvHandleGPU_;
	D3D12_GPU_DESCRIPTOR_HANDLE particleUavHandleGPU_;
	D3D12_GPU_DESCRIPTOR_HANDLE freeListIndexUavHandleGPU_;
	D3D12_GPU_DESCRIPTOR_HANDLE freeListUavHandleGPU_;
	D3D12_GPU_DESCRIPTOR_HANDLE activeCountUavHandleGPU_;

	// SRV/UAV/Texture/Counterインデックス
	uint32_t srvIndex_;
	uint32_t uavIndex_;
	uint32_t freeListIndexUavIndex_;
	uint32_t freeListUavIndex_;
	uint32_t textureIndexSRV_;
	uint32_t activeCountUavIndex_ = 0;
	uint32_t cachedActiveCount_ = 0;


	std::string textureFilePath_;

	// Mesh
	std::shared_ptr<Mesh> mesh_;
	BlendMode blendMode_ = BlendMode::kBlendModeAdd;



	// 統計情報用
	ParticleStats cachedStats_{};
	bool statsInitialized_ = false;
};