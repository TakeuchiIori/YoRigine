#pragma once

// C++
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <dxcapi.h>
#include <string>
#include <vector>
#include <random>
#include <list>
#include <unordered_map >
#include "Loaders/Json/JsonManager.h"
// Engine
#include "Systems./Camera/Camera.h"

// Math
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Mathfunc.h"

class DirectXCommon;
class SrvManager;
class ParticleManager {

public:
	// ブレンドモード構造体
	enum BlendMode {
		// ブレンド無し
		kBlendModeNone,
		// 通常のブレンド
		kBlendModeNormal,
		// 加算
		kBlendModeAdd,
		// 減算
		kBlendModeSubtract,
		// 乗算
		kBlendModeMultiply,
		// スクリーン
		kBlendModeScreen,

		// 利用してはいけない
		kCount0fBlendMode,
	};

	
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};
	struct MaterialData {
		std::string textureFilePath;
		uint32_t textureIndexSRV = 0;
	};

	struct ModelData {
		std::vector<VertexData> vertices;
		MaterialData material;
	};

	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

	struct ParticleForGPU {
		Matrix4x4 WVP;
		Matrix4x4 World;
		Vector4 color;
	};



	struct AccelerationField {
		Vector3 acceleration; // 加速度
		AABB area;			  // 範囲
	};


	struct Particle {
		EulerTransform transform;
		Vector3 velocity;
		Vector4 color;
		float lifeTime;
		float currentTime;
	};

	//enum class ParticleUpdateMode {
	//	kNormal,    // 通常の動き
	//	kThunder,   // 雷
	//	kFire,      // 炎
	//	kSpiral,    // 螺旋
	//	// 他のモードも追加可能
	//};

	struct ParticleGroup {
		MaterialData materialData;										// マテリアルデータ
		std::list<Particle> particles;									// パーティクルリスト
		uint32_t srvIndex;												// インスタンシングデータ用SRVインデックス
		Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;		// インスタンシングリソース
		UINT instance;													// インスタンス数
		ParticleForGPU* instancingData;									// インスタンシングデータを書き込むためのポインタ
	};


	struct ParticleTransformSettings {
		Vector2 scaleX;        // Scale X Min : Max
		Vector2 scaleY;        // Scale Y Min : Max
		Vector2 scaleZ;        // Scale Z Min : Max
		Vector2 translateX;    // Position X Min : Max
		Vector2 translateY;    // Position Y Min : Max
		Vector2 translateZ;    // Position Z Min : Max
		Vector2 rotateX;       // Rotation X Min : Max
		Vector2 rotateY;       // Rotation Y Min : Max
		Vector2 rotateZ;       // Rotation Z Min : Max
	};

	struct ParticleVelocitySettings {
		Vector2 velocityX;     // Velocity X Min : Max
		Vector2 velocityY;     // Velocity Y Min : Max
		Vector2 velocityZ;     // Velocity Z Min : Max
	};

	struct ParticleColorSettings {
		Vector3 minColor;      // Min Color
		Vector3 maxColor;      // Max Color
		float alpha;           // Alpha;
	};


	struct ParticleLifeSettings {
		Vector2 lifeTime;      // Life Time Min : Max
	};


	// 全体のパーティクル構造体の設定
	struct ParticleParameters{
		ParticleTransformSettings baseTransform;
		ParticleVelocitySettings baseVelocity;
		ParticleColorSettings baseColor;
		ParticleLifeSettings baseLife;
	};





public: // シングルトン
	static ParticleManager* GetInstance();
	void Finalize();
	ParticleManager() = default;
	~ParticleManager() = default;
	ParticleManager(const ParticleManager&) = delete;
	ParticleManager& operator=(const ParticleManager&) = delete;

public: // メンバ関数
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(SrvManager* srvManager);

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	/// <summary>
	/// ブレンドモードせてい
	/// </summary>
	/// <param name="blendDesc"></param>
	/// <param name="blendMode"></param>
	void SetBlendMode(D3D12_BLEND_DESC& blendDesc, BlendMode blendMode);

	/// <summary>
	/// ImGuiでブレンドモード変更
	/// </summary>
	/// <param name="currentBlendMode"></param>
	void ShowBlendModeDropdown(BlendMode& currentBlendMode);


	void Render(D3D12_BLEND_DESC& blendDesc, BlendMode& currentBlendMode);

	/// <summary>
	/// パーティクルグループ生成
	/// </summary>
	/// <param name="name"></param>
	/// <param name="textureFilePath"></param>
	void CreateParticleGroup(const std::string name, const std::string textureFilePath);


	/// <summary>
	/// パーティクルの発生
	/// </summary>
	/// <param name="name"></param>
	/// <param name="position"></param>
	/// <param name="count"></param>
	/*void Emit(const std::string& name, const Vector3& position, uint32_t count);*/

	std::list<Particle> Emit(const std::string& name, const Vector3& position, uint32_t count);

private:


	void InitJson(const std::string& name);

	/// <summary>
	/// 横に移動
	/// </summary>
	void UpdateParticleMove();


	/// <summary>
	/// 
	/// </summary>
	void UpdateParticles();

	/// <summary>
	/// 
	/// </summary>
	void UpdateParticlesFor();


	/// <summary>
	/// 行列の更新
	/// </summary>
	void UpadateMatrix();

	/// <summary>
	///  ルートシグネチャ生成
	/// </summary>
	void CreateRootSignature();

	/// <summary>
	/// パイプライン生成
	/// </summary>
	void CreateGraphicsPipeline();

	/// <summary>
	/// 頂点リソース
	/// </summary>
	void CreateVertexResource();

	/// <summary>
	/// マテリアルリソース
	/// </summary>
	void CreateMaterialResource();

	/// <summary>
	/// 頂点バッファビュー
	/// </summary>
	void CreateVertexVBV();

	/// <summary>
	/// パイプラインの設定
	/// </summary>
	void SetGraphicsPipeline();

	/// <summary>
	/// パーティクルの生成
	/// </summary>
	Particle MakeNewParticle(const std::string& name, std::mt19937& randomEngine, const Vector3& position);

	/// <summary>
	/// ImGui
	/// </summary>
	void ShowUpdateModeDropdown();


public:
	void SetCamera(Camera* camera) { camera_ = camera; }

private: // メンバ変数

	// シングルトン
	static std::unique_ptr<ParticleManager> instance;
	static std::once_flag initInstanceFlag;

	// ポインタ
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	VertexData* vertexData_ = nullptr;
	Material* materialData_ = nullptr;
	Camera* camera_ = nullptr;
	ParticleForGPU* instancingData_ = nullptr;
	JsonManager* jsonManager_ = nullptr;

	// ルートシグネチャ
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature_{};
	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing_[1] = {};
	// ルートパラメーター
	D3D12_ROOT_PARAMETER rootParameters_[4] = {};
	// サンプラー
	D3D12_STATIC_SAMPLER_DESC staticSamplers_[1] = {};
	// インプットレイアウト
	D3D12_INPUT_ELEMENT_DESC inputElementDescs_[3] = {};

	// ブレンド
	BlendMode blendMode_{};
	D3D12_BLEND_DESC blendDesc_{};
	D3D12_RASTERIZER_DESC rasterrizerDesc_{};
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc_{};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc_{};
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
	// 頂点リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	// マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob_;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob_;
	

	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;


	// SRV切り替え
	bool useTexture = true;
	bool particleUpdate = false;
	bool useBillboard = true;

	AccelerationField accelerationField;


	ModelData modelData_;
	// 乱数生成器
	std::random_device seedGenerator_;

	// 最初のブレンドモード
	BlendMode currentBlendMode_;
	// パーティクルグループコンテナ
	std::unordered_map<std::string, ParticleGroup> particleGroups_;
	// パラメーター用のコンテナ
	std::unordered_map<std::string, ParticleParameters> particleParameters_;
	const float kDeltaTime = 1.0f / 60.0f;
	// インスタンシング用リソース作成
	const uint32_t kNumMaxInstance = 10000; // インスタンス数
	// ブレンドモードごとのPSOを保持するマップ
	std::unordered_map<BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStates_;

	Matrix4x4 scaleMatrix;
	Matrix4x4 translateMatrix;

	// パーティクル更新モード
	enum ParticleUpdateMode {
		kUpdateModeMove,
		kUpdateModeRadial,
		kUpdateModeSpiral,
	};

	// パーティクル更新モードの選択
	ParticleUpdateMode currentUpdateMode_ = kUpdateModeRadial;

	
};
