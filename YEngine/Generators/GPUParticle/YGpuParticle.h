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
class YGpuParticle
{
public:
	// 定数
	// 1エミッタあたりの最大パーティクル数（＝粒子バッファ長・毎フレーム描画インスタンス数・ディスパッチ上限）。
	// shader 側 Resources/Shaders/Particle/GPUParticle.hlsli の kMaxParticles と必ず一致させること。
	// 5000000: SoA化(hot48+warm64+cold16+freeList4=132B/粒)で 1エミッタ ≈ 629MB。
	// 毎フレーム500万インスタンス描画は死粒もVSで算出→カリングするため重い。実機負荷を要観察。
	static const uint32_t kMaxParticles = 65536;		  // 最大パーティクル数
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

	///************************* GPUバッファ用の構造体（SoA hot/warm/cold） *************************///
	// AoS 176B を hot/warm/cold の3バッファへ分割。shader 側 YGpuParticle.hlsli の
	// ParticleHot / ParticleWarm / ParticleCold と厳密に一致させること。
	// scale / color は保存せず VS で startScale/endScale・startColor/endColor から
	// lifeRatio で lerp 導出する（Update CS の lerp と同一式）。isBillboard は PerView(uniform)。

	// Hot: 毎フレーム Update が書き換える + VS も読む（48B）
	struct ParticleHotGPU {
		Vector3  translate;  float rotate;      // 16
		Vector3  velocity;   float lifeTime;    // 16
		float    currentTime; uint32_t isActive; float pad[2]; // 16
	};
	static_assert(sizeof(ParticleHotGPU) == 48, "ParticleHotGPU must match HLSL ParticleHot (48B)");

	// Warm: Emit 時に確定・寿命中不変。VS が scale/color 導出に読む（64B）
	struct ParticleWarmGPU {
		Vector3  startScale; float pad0;  // 16
		Vector3  endScale;   float pad1;  // 16
		Vector4  startColor;              // 16
		Vector4  endColor;                // 16
	};
	static_assert(sizeof(ParticleWarmGPU) == 64, "ParticleWarmGPU must match HLSL ParticleWarm (64B)");

	// Cold: トレイル生成のみが使う（Update/描画は触らない）（16B）
	struct ParticleColdGPU {
		Vector3  lastTranslate; uint32_t isParent; // 16
	};
	static_assert(sizeof(ParticleColdGPU) == 16, "ParticleColdGPU must match HLSL ParticleCold (16B)");

	struct PerViewForGPU {
		Matrix4x4 viewProjection;
		Matrix4x4 billboardMatrix;
		uint32_t  isBillboard;   // エミッタ単位のビルボード ON/OFF（VSが毎フレーム参照＝切替が即全粒子へ反映）
		float     pad[3];
	};

	// 拡張Paramモジュール（任意ON/OFFの演出）用の共有CBV。
	// VS(g_ExtParams, b1) と Update CS(g_ExtParams, b2) の両方が読む。
	//   VS  が使う: UVScroll / ScalePulse / ColorFlicker / StretchByVelocity（見た目）
	//   CS  が使う: Drag / Bounce（速度・位置の物理更新）
	// HLSL側 ParticleExtParams(GPUParticle.hlsli)と厳密に同一レイアウト (96 bytes)。
	__declspec(align(16))
	struct ParticleExtParameters {
		Vector2  uvScrollSpeed;
		uint32_t uvScrollEnable;
		float    pad0;             // 16

		float    pulseAmplitude;
		float    pulseFrequency;
		uint32_t pulseEnable;
		float    pad1;             // 32

		float    flickerSpeed;
		float    flickerIntensity;
		uint32_t flickerEnable;
		float    pad2;             // 48

		float    dragCoefficient;
		uint32_t dragEnable;
		float    pad3[2];          // 64

		float    stretchScale;
		float    stretchMax;
		uint32_t stretchEnable;
		float    pad4;             // 80

		float    bounceGroundY;
		float    bounceRestitution;
		float    bounceFriction;
		uint32_t bounceEnable;     // 96

		float    emissiveIntensity;
		uint32_t emissiveEnable;
		float    pad5[2];          // 112

		uint32_t flipbookCols;
		uint32_t flipbookRows;
		float    flipbookFps;      // 0以下なら寿命全体でちょうど1周する
		uint32_t flipbookEnable;   // 128
	};
	static_assert(sizeof(ParticleExtParameters) == 128, "ParticleExtParameters must match HLSL ParticleExtParams (128B)");


public:
	///************************* 基本関数 *************************///
	YGpuParticle() = default;
	~YGpuParticle() = default;

	void Initialize(const std::string& filepath, YoRigine::Camera* camera);
	void Update(ID3D12Resource* resource, ID3D12Resource* paramsResource,
		D3D12_GPU_DESCRIPTOR_HANDLE forceFieldSrv, D3D12_GPU_DESCRIPTOR_HANDLE noiseFieldSrv,
		D3D12_GPU_DESCRIPTOR_HANDLE accelerationFieldSrv, bool trailEnabled);
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

	// hot バッファの UAV/SRV（従来 particle と呼んでいたものが hot に相当）
	D3D12_GPU_DESCRIPTOR_HANDLE GetParticleUavHandleGPU() const { return particleUavHandleGPU_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetParticleSrvHandleGPU() const { return particleSrvHandleGPU_; }
	// SoA warm/cold の UAV（Init/Emit/Trail が書き込む）
	D3D12_GPU_DESCRIPTOR_HANDLE GetWarmUavHandleGPU() const { return warmUavHandleGPU_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetColdUavHandleGPU() const { return coldUavHandleGPU_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetFreeListIndexUavHandleGPU() const { return freeListIndexUavHandleGPU_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetFreeListUavHandleGPU() const { return freeListUavHandleGPU_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetActiveCountUavHandleGPU() const { return activeCountUavHandleGPU_; }

	ID3D12Resource* GetParticleResource() const { return particleResource_.Get(); }
	ID3D12Resource* GetWarmResource() const { return warmResource_.Get(); }
	ID3D12Resource* GetColdResource() const { return coldResource_.Get(); }
	ID3D12Resource* GetFreeListIndexResource() const { return freeListIndexResource_.Get(); }
	ID3D12Resource* GetFreeListResource() const { return freeListResource_.Get(); }
	ID3D12Resource* GetActiveCountResource() const { return activeCountResource_.Get(); }
	ID3D12Resource* GetPerViewResource() const { return perViewResource_.Get(); }
	ID3D12Resource* GetVertexResource() const { return mesh_->GetMeshResource().vertexResource.Get(); }

	PerViewForGPU* GetPerViewData() const { return perViewData_; }

	// 拡張Paramモジュール（VS b1）。呼ぶたびに即座にマップ済みバッファへ反映される。
	void SetExtParams(const ParticleExtParameters& params) { *extParamsData_ = params; }

	void SetMesh(std::shared_ptr<Mesh> mesh) { mesh_ = mesh; }
	void SetCamera(YoRigine::Camera* camera) { camera_ = camera; }
	// エミッタ単位のビルボード ON/OFF（次フレームの UpdatePerView で全粒子へ反映）
	void SetBillboard(bool enable) { billboard_ = enable; }



	//--------------------------------- マテリアル関連 ---------------------------------//
	Vector4& GetColor() { return materialColor_->GetColor(); }
	void SetMaterialColor(const Vector4& color) { materialColor_->SetColor(color); }
	void SetAlpha(float alpha) { materialColor_->SetAlpha(alpha); }
	void SetUvTransform(const Matrix4x4& uvTransform) { materialUV_->SetUVTransform(uvTransform); }

	//--------------------------------- テクスチャ関連 ---------------------------------//
	// 実行時にテクスチャを差し替える（YParticleSystem::SetTexture と同じ用途）
	void SetTexture(const std::string& textureFilePath);
	const std::string& GetTexturePath() const { return textureFilePath_; }
private:
	///************************* 内部処理 *************************///
	void CreateYGpuParticleResource();
	void CreatePerViewResource();
	void CreateExtParamsResource();
	void CreateVertexResource();

	void CreateUAV();
	void CreateDrawIndirectResources(); // drawList/drawArgs バッファ＋コマンドシグネチャ生成
	void CreateTexture();

	void UpdateMaterial();
	void UpdateLight();
	void UpdatePerView();

	void DispatchInit();
	void DispatchUpdate(ID3D12Resource* resource, ID3D12Resource* paramsResource,
		D3D12_GPU_DESCRIPTOR_HANDLE forceFieldSrv, D3D12_GPU_DESCRIPTOR_HANDLE noiseFieldSrv,
		D3D12_GPU_DESCRIPTOR_HANDLE accelerationFieldSrv, bool trailEnabled);

private:
	///************************* メンバ変数 *************************///
	YoRigine::DirectXCommon* dxCommon_;
	YPipelineManager* pipelineManager_;
	ComputeShaderManager* computeShaderManager_;
	YoRigine::Camera* camera_;

	// マテリアル関連
	std::unique_ptr<MaterialColor> materialColor_;
	std::unique_ptr<MaterialUV> materialUV_;

	// データポインタ
	PerViewForGPU* perViewData_;
	ParticleExtParameters* extParamsData_ = nullptr;

	// リソース（particleResource_ = hot バッファ）
	Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> warmResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> coldResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> extParamsResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> activeCountResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> activeCountReadback_;
	// インダイレクト描画: 生存粒子だけを描画するためのコンパクトリスト＋描画引数
	Microsoft::WRL::ComPtr<ID3D12Resource> drawListResource_;      // uint[max] 生存スロット番号
	Microsoft::WRL::ComPtr<ID3D12Resource> drawArgsResource_;      // D3D12_DRAW_INDEXED_ARGUMENTS(5 uint)。[1]=生存数カウンタ兼InstanceCount
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> drawIndirectCmdSig_;

	// SRV/UAVハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE particleSrvHandleGPU_; // hot SRV（VS t0）
	D3D12_GPU_DESCRIPTOR_HANDLE warmSrvHandleGPU_;     // warm SRV（VS t1・scale/color 導出用）
	D3D12_GPU_DESCRIPTOR_HANDLE particleUavHandleGPU_; // hot UAV（u0）
	D3D12_GPU_DESCRIPTOR_HANDLE warmUavHandleGPU_;     // warm UAV（u4）
	D3D12_GPU_DESCRIPTOR_HANDLE coldUavHandleGPU_;     // cold UAV（u5）
	D3D12_GPU_DESCRIPTOR_HANDLE freeListIndexUavHandleGPU_;
	D3D12_GPU_DESCRIPTOR_HANDLE freeListUavHandleGPU_;
	D3D12_GPU_DESCRIPTOR_HANDLE activeCountUavHandleGPU_;
	D3D12_GPU_DESCRIPTOR_HANDLE drawListUavHandleGPU_; // drawList UAV（u6）
	D3D12_GPU_DESCRIPTOR_HANDLE drawArgsUavHandleGPU_; // drawArgs UAV（u7）
	D3D12_GPU_DESCRIPTOR_HANDLE drawListSrvHandleGPU_; // drawList SRV（VS t2）

	// SRV/UAV/Texture/Counterインデックス
	uint32_t srvIndex_;       // hot SRV
	uint32_t warmSrvIndex_;   // warm SRV
	uint32_t uavIndex_;       // hot UAV
	uint32_t warmUavIndex_;   // warm UAV
	uint32_t coldUavIndex_;   // cold UAV
	uint32_t freeListIndexUavIndex_;
	uint32_t freeListUavIndex_;
	uint32_t drawListUavIndex_;
	uint32_t drawArgsUavIndex_;
	uint32_t drawListSrvIndex_;
	uint32_t textureIndexSRV_;
	uint32_t activeCountUavIndex_ = 0;
	uint32_t cachedActiveCount_ = 0;


	std::string textureFilePath_;

	// Mesh
	std::shared_ptr<Mesh> mesh_;
	BlendMode blendMode_ = BlendMode::kBlendModeAdd;

	// ビルボード（エミッタ単位・毎フレーム PerView に反映）
	bool billboard_ = true;



	// 統計情報用
	ParticleStats cachedStats_{};
	bool statsInitialized_ = false;
};