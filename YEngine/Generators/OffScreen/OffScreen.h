#pragma once

// C++
#include <wrl.h>
#include <d3d12.h>
#include <unordered_map>

#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector4.h"

#include "WinApp/WinApp.h"


namespace YoRigine {
	class DirectXCommon;
}


/// <summary>
/// オフスクリーン生成クラス
/// </summary>
class OffScreen
{
public:

	// 利用するエフェクトの種類
	enum class OffScreenEffectType {
		Copy,
		GaussSmoothing,
		DepthOutline,
		Sepia,
		Grayscale,
		Vignette,
		RadialBlur,
		ToneMapping,
		Dissolve,
		Chromatic,
		ColorAdjust,
		ShatterTransition,
	};
	///************************* パラメータ調整 *************************///
	struct RadialBlurPrams
	{
		Vector2 direction = { 0.0f, 0.0f };
		Vector2 center = { 0.5f, 0.5f };
		float width = 0.001f;
		int sampleCount = 10;
		bool isRadial = true;
	};

	struct DissolveParams {
		float threshold;
		float edgeWidth;
		Vector3 edgeColor;
		float invert;
	};

	static OffScreen* GetInstance() {
		static OffScreen instance;
		return &instance;
	}

	struct ChromaticParams {
		float aberrationStrength = 0.0f;
		Vector2 screenSize = { 0.0f,0.0f };
		float edgeStrength = 0.0f;
	};

	struct ColorAdjustParams {
		float brightness = 0.0f;        // 明るさ
		float contrast = 1.0f;          // コントラスト
		float saturation = 1.0f;        // 彩度
		float hue = 0.0f;               // 色相
	};

	struct ToneParams {
		float gamma = 2.2f;             // ガンマ補正
		float exposure = 1.0f;          // エクスポージャー
	};

	struct ShatterTransitionParams {
		float progress = 0.0f;
		Vector2 resolution = { 0.0f,0.0f };
		float time = 0.0f;
	};

	///************************* 基本関数 *************************///

	// 初期化
	void Initialize();

	// 指定されたエフェクトで描画（新しいメインインターフェース）
	void RenderEffect(OffScreenEffectType type, D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);

	///************************* パラメータ設定 *************************///

	// プロジェクション行列のセット
	void SetProjection(const Matrix4x4& projectionMatrix) { projectionInverse_ = projectionMatrix; }

	// 全開放
	void ReleaseResources();

	// トーンマッピングのパラメータを設定
	void SetToneMappingExposure(float exposure);

	// ガウシアンブラーのパラメータを設定
	void SetGaussianBlurParams(float sigma, int kernelSize);

	// デプスアウトラインのパラメータを設定
	void SetDepthOutlineParams(int kernelSize, const Vector4& color);

	// ラジアルブラーのパラメータを設定
	void SetRadialBlurParams(const RadialBlurPrams& params);

	// ディゾルブのパラメータを設定
	void SetDissolveParams(const DissolveParams& params);

	// クロマチックアバーレーションのパラメータを設定
	void SetChromaticParams(const ChromaticParams& params);

	// 色調整のパラメータを設定
	void SetColorAdjustParams(const ColorAdjustParams& colorParams, const ToneParams& toneParams);

	// 破壊シーン遷移のパラメータを設定
	void SetShatterTransitionParams(const ShatterTransitionParams& params);
	///************************* ゲーム用機能（時間経過ブラー） *************************///

	// ブラーの更新（時間経過による減衰）
	void UpdateBlur(float deltaTime);

	// ブラー演出を開始する（最初強→最後弱）
	void StartBlurMotion(const RadialBlurPrams& params);

	// ブラー演出が実行中かどうか
	bool IsBlurMotionActive() const { return isBlurMotion_; }

	// ブラー演出を停止
	void StopBlurMotion() { isBlurMotion_ = false; }

private:
	///************************* 内部処理 *************************///

	OffScreen() = default;
	~OffScreen() = default;
	OffScreen(const OffScreen&) = delete;
	OffScreen& operator=(const OffScreen&) = delete;

	// リソース作成
	void CreateAllResources();
	void CreateBoxFilterResource();
	void CreateGaussFilterResource();
	void CreateDepthOutLineResource();
	void CreateRadialBlurResource();
	void CreateToneMappingResource();
	void CreateDissolveResource();
	void CreateChromaticResource();
	void CreateColorAdjustResource();
	void CreateShatterTransitionResource();

	// エフェクト別の描画処理
	void ExecuteCopyEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteGaussSmoothingEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteDepthOutlineEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteSepiaEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteGrayscaleEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteVignetteEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteRadialBlurEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteToneMappingEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteDissolveEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteChromaticEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteColorAdjustEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteShatterTransitionEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);

	// 共通描画処理
	void SetupPipelineAndDraw(OffScreenEffectType type);

private:
	///************************* GPU リソース構造体 *************************///

	struct KernelForGPU {
		int kernelSize;
		int padding[3];
	};

	struct GaussKernelForGPU {
		int kernelSize;
		float sigma;
		float padding[2];
	};

	struct Material {
		Matrix4x4 Inverse;
		int kernelSize;
		int padding[3];
		Vector4 outlineColor;
	};

	struct RadialBlurForGPU {
		Vector2 direction;
		Vector2 center;
		float width;
		int sampleCount;
		bool isRadial;
		float padding[1];
	};

	struct ToneMappingForGPU {
		float exposure;
		float padding[3];
	};

	struct DissolveForGPU
	{
		float threshold;
		float edgeWidth;
		float padding[2];

		Vector3 edgeColor;
		float invert;

		float padding1[3];
	};

	struct ChromaticForGPU {
		float aberrationStrength;
		Vector2 screenSize;
		float edgeStrength;
	};


	struct ColorAdjustForGPU {
		float brightness;
		float contrast;
		float saturation;
		float hue;
	};

	struct ToneParamsForGPU {
		float gamma;
		float exposure;
		float padding[2];
	};
	struct ShatterTransitionForGPU {
		float progress;
		Vector2 resolution;
		float time;
		float padding;
	};


	///************************* パイプライン管理 *************************///

	struct OffScreenPipeline {
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	};

	YoRigine::DirectXCommon* dxCommon_ = nullptr;
	std::unordered_map<OffScreenEffectType, OffScreenPipeline> pipelineMap_;

	///************************* GPU リソース *************************///

	// ぼかし用
	Microsoft::WRL::ComPtr<ID3D12Resource> boxResource_;
	KernelForGPU* boxData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> gaussResource_;
	GaussKernelForGPU* gaussData_ = nullptr;

	// デプスアウトライン用
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	Matrix4x4 projectionInverse_;

	// ラジアルブラー用
	Microsoft::WRL::ComPtr<ID3D12Resource> radialBlurResource_;
	RadialBlurForGPU* radialBlurData_ = nullptr;

	// トーンマッピング用
	Microsoft::WRL::ComPtr<ID3D12Resource> toneMappingResource_;
	ToneMappingForGPU* toneMappingData_ = nullptr;

	// ディゾルブ用
	Microsoft::WRL::ComPtr<ID3D12Resource> dissolveResource_;
	DissolveForGPU* dissolveData_ = nullptr;
	std::string maskTexturePath_ = "Resources/images/noise0.png";

	// クロマチックアバーレーション用
	Microsoft::WRL::ComPtr<ID3D12Resource> chromaticResource_;
	ChromaticForGPU* chromaticData_ = nullptr;

	// 色調整用
	Microsoft::WRL::ComPtr<ID3D12Resource> colorAdjustResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> toneParamsResource_;
	ColorAdjustForGPU* colorAdjustData_ = nullptr;
	ToneParamsForGPU* toneParamsData_ = nullptr;

	// 破壊シーン遷移用
	Microsoft::WRL::ComPtr<ID3D12Resource> shatterTransitionResource_;
	ShatterTransitionForGPU* shatterTransitionData_ = nullptr;
	std::string shatterTexturePath_ = "Resources/images/break.png";

	///************************* ブラー演出用 *************************///

	RadialBlurPrams radialBlurPrams_;
	bool isBlurMotion_ = false;
	float blurTime_ = 0.0f;
	float blurDuration_ = 1.0f;		// ブラー時間（秒）
	float initialWidth_ = 0.01f;	// ブラー初期幅
	int initialSampleCount_ = 16;

	///************************* 破壊シーン遷移用 *************************///

	ShatterTransitionParams shatterParams_;
};